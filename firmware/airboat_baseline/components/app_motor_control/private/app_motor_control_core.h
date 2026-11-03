// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_motor_control.h"

typedef struct {
    app_motor_safety_handle_t safety;
    app_motor_safety_snapshot_t snapshot;
} app_motor_control_core_t;

app_motor_control_result_t app_motor_control_core_init(app_motor_control_core_t *core,
                                                        uint32_t now_ms);
app_motor_control_result_t app_motor_control_core_apply(
    app_motor_control_core_t *core, app_motor_control_action_t action);
app_motor_control_result_t app_motor_control_core_submit(
    app_motor_control_core_t *core, const app_motor_safety_command_t *command,
    uint32_t received_ms);
app_motor_control_result_t app_motor_control_core_tick(app_motor_control_core_t *core,
                                                        uint32_t now_ms);
void app_motor_control_core_deinit(app_motor_control_core_t *core);
