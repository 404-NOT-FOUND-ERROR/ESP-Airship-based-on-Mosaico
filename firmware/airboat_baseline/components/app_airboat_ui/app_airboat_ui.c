// SPDX-License-Identifier: Apache-2.0

#include "app_airboat_ui.h"

#include <stdio.h>
#include <stdlib.h>

#include "bundle_gsp.h"

#define AIRBOAT_TICK_MS 50U

struct app_airboat_ui {
    esp_gsp_handle_t ui;
    app_motor_control_handle_t control;
    app_motor_safety_snapshot_t snapshot;
    void *timer;
    esp_gsp_err_t last_error;
};

static void remember_error(struct app_airboat_ui *state, esp_gsp_err_t err)
{
    if (err != ESP_GSP_OK) {
        state->last_error = err;
    }
}

static const char *axis_text(const app_motor_safety_motor_output_t *motor,
                             char *buffer, size_t size)
{
    snprintf(buffer, size, "%u / %u", (unsigned)motor->input_a_permille,
             (unsigned)motor->input_b_permille);
    return buffer;
}

static void refresh_ui(struct app_airboat_ui *state)
{
    if (app_motor_control_read(state->control, &state->snapshot) != APP_MOTOR_CONTROL_OK) {
        remember_error(state, ESP_GSP_ERR_INVALID_STATE);
        return;
    }

    const char *status = "DISARMED";
    const char *detail = "SAFE / OUTPUTS ZERO";
    if (state->snapshot.emergency_stop_latched) {
        status = "E-STOP LOCKED";
        detail = "CLEAR ESTOP TO RESET";
    } else if (state->snapshot.armed) {
        status = "ARMED / PWM OFF";
        detail = state->snapshot.command_fresh ? "COMMAND FRESH" : "WAITING COMMAND";
    }

    char propulsion[24];
    char yaw[24];
    char lift[24];
    char watchdog[24];
    remember_error(state, gsp_airboat_status_set_text(state->ui, status));
    remember_error(state, gsp_airboat_detail_set_text(state->ui, detail));
    remember_error(state, gsp_airboat_propulsion_set_text(
                            state->ui, axis_text(&state->snapshot.motor[0], propulsion,
                                                 sizeof(propulsion))));
    remember_error(state, gsp_airboat_yaw_set_text(
                            state->ui, axis_text(&state->snapshot.motor[1], yaw,
                                                 sizeof(yaw))));
    remember_error(state, gsp_airboat_lift_set_text(
                            state->ui, axis_text(&state->snapshot.motor[2], lift,
                                                 sizeof(lift))));
    snprintf(watchdog, sizeof(watchdog), "WATCHDOG %ums",
             (unsigned)APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS);
    remember_error(state, gsp_airboat_watchdog_set_text(state->ui, watchdog));
}

static void airboat_tick(esp_gsp_handle_t ui, void *ctx)
{
    (void)ui;
    struct app_airboat_ui *state = ctx;
    refresh_ui(state);
}

static void airboat_event(esp_gsp_handle_t ui, const esp_gsp_event_t *event,
                          void *ctx)
{
    (void)ui;
    struct app_airboat_ui *state = ctx;
    app_motor_control_action_t action;
    if (gsp_airboat_event_is_arm(event)) {
        action = APP_MOTOR_CONTROL_ACTION_ARM;
    } else if (gsp_airboat_event_is_disarm(event)) {
        action = APP_MOTOR_CONTROL_ACTION_DISARM;
    } else if (gsp_airboat_event_is_estop(event)) {
        action = APP_MOTOR_CONTROL_ACTION_ESTOP;
    } else if (gsp_airboat_event_is_clear_estop(event)) {
        action = APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP;
    } else {
        return;
    }
    if (app_motor_control_post(state->control, action) != APP_MOTOR_CONTROL_OK) {
        remember_error(state, ESP_GSP_ERR_INVALID_STATE);
        return;
    }
    refresh_ui(state);
}

esp_gsp_err_t app_airboat_ui_create(esp_gsp_handle_t ui,
                                    app_motor_control_handle_t control,
                                    app_airboat_ui_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;
    if (!ui || !control) {
        return ESP_GSP_ERR_INVALID_ARG;
    }

    struct app_airboat_ui *state = calloc(1, sizeof(*state));
    if (!state) {
        return ESP_GSP_ERR_NO_MEM;
    }
    state->ui = ui;
    state->control = control;
    refresh_ui(state);
    if (state->last_error != ESP_GSP_OK) {
        const esp_gsp_err_t err = state->last_error;
        free(state);
        return err;
    }

    esp_gsp_err_t err = esp_gsp_on_event(ui, airboat_event, state);
    if (err != ESP_GSP_OK) {
        free(state);
        return err;
    }
    state->timer = esp_gsp_timer_create(ui, AIRBOAT_TICK_MS, airboat_tick, state);
    if (!state->timer) {
        const esp_gsp_err_t detach_err = esp_gsp_on_event(ui, NULL, NULL);
        if (detach_err == ESP_GSP_OK) {
            free(state);
            return ESP_GSP_ERR_NO_MEM;
        }
        *out_handle = state;
        return detach_err;
    }
    *out_handle = state;
    return ESP_GSP_OK;
}

esp_gsp_err_t app_airboat_ui_read(app_airboat_ui_handle_t handle,
                                  app_airboat_ui_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *out_snapshot = (app_airboat_ui_snapshot_t) {
        .safety = handle->snapshot,
        .last_error = handle->last_error,
    };
    return ESP_GSP_OK;
}

esp_gsp_err_t app_airboat_ui_destroy(app_airboat_ui_handle_t *handle)
{
    if (!handle) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    struct app_airboat_ui *state = *handle;
    if (!state) {
        return ESP_GSP_OK;
    }
    (void)app_motor_control_post(state->control, APP_MOTOR_CONTROL_ACTION_DISARM);
    esp_gsp_err_t err = esp_gsp_on_event(state->ui, NULL, NULL);
    if (err != ESP_GSP_OK) {
        return err;
    }
    if (state->timer) {
        err = esp_gsp_timer_delete(state->ui, state->timer);
        if (err != ESP_GSP_OK) {
            return err;
        }
    }
    free(state);
    *handle = NULL;
    return ESP_GSP_OK;
}
