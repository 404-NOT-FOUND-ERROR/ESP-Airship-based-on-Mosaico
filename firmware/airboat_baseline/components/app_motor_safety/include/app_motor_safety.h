// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MOTOR_SAFETY_AXIS_COUNT 3
#define APP_MOTOR_SAFETY_DUTY_MAX 1000
#define APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS 250U
#define APP_MOTOR_SAFETY_REVERSAL_DEADTIME_MS 50U

typedef struct app_motor_safety *app_motor_safety_handle_t;

typedef enum {
    APP_MOTOR_SAFETY_PROPULSION = 0,
    APP_MOTOR_SAFETY_YAW = 1,
    APP_MOTOR_SAFETY_LIFT = 2,
} app_motor_safety_axis_t;

typedef enum {
    APP_MOTOR_SAFETY_OK = 0,
    APP_MOTOR_SAFETY_INVALID_ARGUMENT,
    APP_MOTOR_SAFETY_NO_MEMORY,
    APP_MOTOR_SAFETY_INVALID_STATE,
} app_motor_safety_result_t;

typedef struct {
    int16_t axis[APP_MOTOR_SAFETY_AXIS_COUNT]; /**< -1000..1000 per axis. */
} app_motor_safety_command_t;

typedef struct {
    uint16_t input_a_permille; /**< First input of this H-bridge, 0..1000. */
    uint16_t input_b_permille; /**< Second input of this H-bridge, 0..1000. */
} app_motor_safety_motor_output_t;

typedef struct {
    app_motor_safety_motor_output_t motor[APP_MOTOR_SAFETY_AXIS_COUNT];
    bool armed;
    bool emergency_stop_latched;
    bool command_fresh;
    uint32_t command_age_ms;
} app_motor_safety_snapshot_t;

/** Create an inhibited controller; the caller owns the returned handle. */
app_motor_safety_result_t app_motor_safety_create(app_motor_safety_handle_t *out_handle);

/** Arm command processing without restoring a previous command. */
app_motor_safety_result_t app_motor_safety_arm(app_motor_safety_handle_t handle);

/** Disarm and invalidate the command; the next tick returns 0/0. */
app_motor_safety_result_t app_motor_safety_disarm(app_motor_safety_handle_t handle);

/** Latch emergency stop and disarm. Clearing requires a separate disarmed call. */
app_motor_safety_result_t app_motor_safety_latch_emergency_stop(
    app_motor_safety_handle_t handle);

/** Clear a latched stop only while disarmed; the controller remains disarmed. */
app_motor_safety_result_t app_motor_safety_clear_emergency_stop(
    app_motor_safety_handle_t handle);

/** Submit a signed command; an expired session disarms and rejects the command. */
app_motor_safety_result_t app_motor_safety_submit(
    app_motor_safety_handle_t handle,
    const app_motor_safety_command_t *command,
    uint32_t now_ms);

/** Apply safety timing and return the 0/0-coast DRV8833 input duties. */
app_motor_safety_result_t app_motor_safety_tick(
    app_motor_safety_handle_t handle,
    uint32_t now_ms,
    app_motor_safety_snapshot_t *out_snapshot);

/** Release the controller and clear the caller's handle. */
app_motor_safety_result_t app_motor_safety_destroy(app_motor_safety_handle_t *handle);

#ifdef __cplusplus
}
#endif
