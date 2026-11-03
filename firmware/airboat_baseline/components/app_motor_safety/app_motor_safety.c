// SPDX-License-Identifier: Apache-2.0

#include "app_motor_safety.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int8_t last_direction;
    bool active;
    bool restart_deadtime_pending;
    bool zero_time_valid;
    uint32_t zero_since_ms;
    uint16_t duty;
} motor_state_t;

struct app_motor_safety {
    bool armed;
    bool emergency_stop_latched;
    bool command_valid;
    uint32_t command_received_ms;
    int16_t target[APP_MOTOR_SAFETY_AXIS_COUNT];
    motor_state_t motor[APP_MOTOR_SAFETY_AXIS_COUNT];
};

static void stop_motor_states(struct app_motor_safety *handle)
{
    for (size_t i = 0; i < APP_MOTOR_SAFETY_AXIS_COUNT; ++i) {
        motor_state_t *state = &handle->motor[i];
        state->active = false;
        state->duty = 0;
        /* Preserve direction history so disarm -> rearm cannot skip a stop. */
        state->restart_deadtime_pending = state->last_direction != 0;
        state->zero_time_valid = false;
    }
}

static int16_t clamp_command(int16_t value)
{
    if (value > APP_MOTOR_SAFETY_DUTY_MAX) {
        return APP_MOTOR_SAFETY_DUTY_MAX;
    }
    if (value < -APP_MOTOR_SAFETY_DUTY_MAX) {
        return -APP_MOTOR_SAFETY_DUTY_MAX;
    }
    return value;
}

static uint16_t magnitude(int16_t value)
{
    return (uint16_t)(value < 0 ? -(int32_t)value : value);
}

static void update_motor(motor_state_t *state, int16_t command, uint32_t now_ms,
                         app_motor_safety_motor_output_t *out)
{
    const int8_t direction = command > 0 ? 1 : (command < 0 ? -1 : 0);
    const uint16_t duty = magnitude(command);

    /* Keep both bridge inputs low before changing the remembered direction. */
    if (state->active && direction != state->last_direction) {
        state->active = false;
        state->duty = 0;
        state->zero_since_ms = now_ms;
        state->zero_time_valid = true;
    }

    if (direction == 0) {
        if (state->active) {
            state->active = false;
            state->zero_since_ms = now_ms;
            state->zero_time_valid = true;
        }
        if (state->restart_deadtime_pending && !state->zero_time_valid) {
            state->zero_since_ms = now_ms;
            state->zero_time_valid = true;
        }
        state->duty = 0;
    } else if (!state->active) {
        bool can_start = true;
        if (state->restart_deadtime_pending) {
            if (!state->zero_time_valid) {
                state->zero_since_ms = now_ms;
                state->zero_time_valid = true;
            }
            if (now_ms - state->zero_since_ms <
                APP_MOTOR_SAFETY_REVERSAL_DEADTIME_MS) {
                can_start = false;
            } else {
                state->restart_deadtime_pending = false;
                state->zero_time_valid = false;
            }
        }
        if (can_start) {
            const bool reversing = state->last_direction != 0 &&
                                   direction != state->last_direction;
            const uint32_t zero_duration = now_ms - state->zero_since_ms;
            if (!reversing || !state->zero_time_valid ||
                zero_duration >= APP_MOTOR_SAFETY_REVERSAL_DEADTIME_MS) {
                state->last_direction = direction;
                state->active = true;
                state->zero_time_valid = false;
                state->duty = duty;
            } else {
                state->duty = 0;
            }
        } else {
            state->duty = 0;
        }
    } else {
        state->duty = duty;
    }

    out->input_a_permille = state->active && state->last_direction > 0
                                ? state->duty
                                : 0;
    out->input_b_permille = state->active && state->last_direction < 0
                                ? state->duty
                                : 0;
}

app_motor_safety_result_t app_motor_safety_create(app_motor_safety_handle_t *out_handle)
{
    if (!out_handle) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    *out_handle = NULL;
    struct app_motor_safety *state = calloc(1, sizeof(*state));
    if (!state) {
        return APP_MOTOR_SAFETY_NO_MEMORY;
    }
    *out_handle = state;
    return APP_MOTOR_SAFETY_OK;
}

app_motor_safety_result_t app_motor_safety_arm(app_motor_safety_handle_t handle)
{
    if (!handle) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    if (handle->emergency_stop_latched) {
        return APP_MOTOR_SAFETY_INVALID_STATE;
    }
    handle->armed = true;
    handle->command_valid = false;
    memset(handle->target, 0, sizeof(handle->target));
    stop_motor_states(handle);
    return APP_MOTOR_SAFETY_OK;
}

app_motor_safety_result_t app_motor_safety_disarm(app_motor_safety_handle_t handle)
{
    if (!handle) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    handle->armed = false;
    handle->command_valid = false;
    memset(handle->target, 0, sizeof(handle->target));
    stop_motor_states(handle);
    return APP_MOTOR_SAFETY_OK;
}

app_motor_safety_result_t app_motor_safety_latch_emergency_stop(
    app_motor_safety_handle_t handle)
{
    if (!handle) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    handle->emergency_stop_latched = true;
    return app_motor_safety_disarm(handle);
}

app_motor_safety_result_t app_motor_safety_clear_emergency_stop(
    app_motor_safety_handle_t handle)
{
    if (!handle) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    if (handle->armed) {
        return APP_MOTOR_SAFETY_INVALID_STATE;
    }
    handle->emergency_stop_latched = false;
    handle->command_valid = false;
    memset(handle->target, 0, sizeof(handle->target));
    stop_motor_states(handle);
    return APP_MOTOR_SAFETY_OK;
}

app_motor_safety_result_t app_motor_safety_submit(
    app_motor_safety_handle_t handle,
    const app_motor_safety_command_t *command,
    uint32_t now_ms)
{
    if (!handle || !command) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    if (!handle->armed || handle->emergency_stop_latched) {
        return APP_MOTOR_SAFETY_INVALID_STATE;
    }
    if (handle->command_valid &&
        now_ms - handle->command_received_ms >= APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS) {
        /* Do not let a late packet revive an expired session between ticks. */
        (void)app_motor_safety_disarm(handle);
        return APP_MOTOR_SAFETY_INVALID_STATE;
    }
    for (size_t i = 0; i < APP_MOTOR_SAFETY_AXIS_COUNT; ++i) {
        handle->target[i] = clamp_command(command->axis[i]);
    }
    handle->command_received_ms = now_ms;
    handle->command_valid = true;
    return APP_MOTOR_SAFETY_OK;
}

app_motor_safety_result_t app_motor_safety_tick(
    app_motor_safety_handle_t handle,
    uint32_t now_ms,
    app_motor_safety_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }

    const uint32_t age_ms = now_ms - handle->command_received_ms;
    bool command_fresh = handle->armed && handle->command_valid &&
                         age_ms < APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS;
    if (handle->armed && handle->command_valid && !command_fresh) {
        /* A stale command is a control-session failure, so require a new arm. */
        (void)app_motor_safety_disarm(handle);
        command_fresh = false;
    }
    app_motor_safety_motor_output_t output[APP_MOTOR_SAFETY_AXIS_COUNT];
    for (size_t i = 0; i < APP_MOTOR_SAFETY_AXIS_COUNT; ++i) {
        update_motor(&handle->motor[i], command_fresh ? handle->target[i] : 0,
                     now_ms, &output[i]);
    }
    *out_snapshot = (app_motor_safety_snapshot_t) {
        .armed = handle->armed,
        .emergency_stop_latched = handle->emergency_stop_latched,
        .command_fresh = command_fresh,
        .command_age_ms = handle->command_valid ? age_ms : 0,
    };
    memcpy(out_snapshot->motor, output, sizeof(output));
    return APP_MOTOR_SAFETY_OK;
}

app_motor_safety_result_t app_motor_safety_destroy(app_motor_safety_handle_t *handle)
{
    if (!handle) {
        return APP_MOTOR_SAFETY_INVALID_ARGUMENT;
    }
    free(*handle);
    *handle = NULL;
    return APP_MOTOR_SAFETY_OK;
}
