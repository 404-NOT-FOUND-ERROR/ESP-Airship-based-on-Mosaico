// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_motor_control.h"
#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app_airboat_ui *app_airboat_ui_handle_t;

typedef struct {
    app_motor_safety_snapshot_t safety;
    esp_gsp_err_t last_error;
} app_airboat_ui_snapshot_t;

/** Create the GSP view; the caller owns the independent control service. */
esp_gsp_err_t app_airboat_ui_create(esp_gsp_handle_t ui,
                                    app_motor_control_handle_t control,
                                    app_airboat_ui_handle_t *out_handle);

/** Copy the last safety snapshot; call on the GSP owner loop. */
esp_gsp_err_t app_airboat_ui_read(app_airboat_ui_handle_t handle,
                                  app_airboat_ui_snapshot_t *out_snapshot);

/** Detach callbacks and release the timer and safety controller. */
esp_gsp_err_t app_airboat_ui_destroy(app_airboat_ui_handle_t *handle);

#ifdef __cplusplus
}
#endif
