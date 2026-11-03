// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

#include "app_airboat_ui.h"
#include "gsp_sim_bridge.h"

static app_airboat_ui_handle_t s_controller;
static app_motor_control_handle_t s_control;

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    if (app_motor_control_create(&s_control) != APP_MOTOR_CONTROL_OK) {
        return ESP_GSP_ERR_NO_MEM;
    }
    const esp_gsp_err_t err = app_airboat_ui_create(ui, s_control, &s_controller);
    if (err != ESP_GSP_OK && !s_controller) {
        (void)app_motor_control_destroy(&s_control);
    }
    return err;
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    (void)ui;
    app_airboat_ui_snapshot_t snapshot;
    if (app_airboat_ui_read(s_controller, &snapshot) == ESP_GSP_OK) {
        fprintf(stderr, "airboat_ui: armed=%d estop=%d fresh=%d\n",
                snapshot.safety.armed,
                snapshot.safety.emergency_stop_latched,
                snapshot.safety.command_fresh);
    }
    const esp_gsp_err_t err = app_airboat_ui_destroy(&s_controller);
    if (err != ESP_GSP_OK) {
        fprintf(stderr, "airboat_ui: cleanup failed (%d)\n", err);
        return;
    }
    if (app_motor_control_destroy(&s_control) != APP_MOTOR_CONTROL_OK) {
        fprintf(stderr, "airboat_ui: control task did not stop cleanly\n");
    }
}
