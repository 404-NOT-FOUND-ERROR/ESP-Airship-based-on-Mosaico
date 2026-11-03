// SPDX-License-Identifier: Apache-2.0
#include "app_motor_control_core.h"

#include <string.h>

app_motor_control_result_t app_motor_control_core_init(app_motor_control_core_t *core,
                                                        uint32_t now_ms)
{
    if (!core) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    memset(core, 0, sizeof(*core));
    if (app_motor_safety_create(&core->safety) != APP_MOTOR_SAFETY_OK) {
        return APP_MOTOR_CONTROL_NO_MEMORY;
    }
    if (app_motor_safety_tick(core->safety, now_ms, &core->snapshot) != APP_MOTOR_SAFETY_OK) {
        app_motor_safety_destroy(&core->safety);
        return APP_MOTOR_CONTROL_INVALID_STATE;
    }
    return APP_MOTOR_CONTROL_OK;
}

app_motor_control_result_t app_motor_control_core_apply(
    app_motor_control_core_t *core, app_motor_control_action_t action)
{
    if (!core || !core->safety) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }

    app_motor_safety_result_t result;
    switch (action) {
    case APP_MOTOR_CONTROL_ACTION_ARM:
        result = app_motor_safety_arm(core->safety);
        break;
    case APP_MOTOR_CONTROL_ACTION_DISARM:
        result = app_motor_safety_disarm(core->safety);
        break;
    case APP_MOTOR_CONTROL_ACTION_ESTOP:
        result = app_motor_safety_latch_emergency_stop(core->safety);
        break;
    case APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP:
        result = app_motor_safety_clear_emergency_stop(core->safety);
        break;
    default:
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    return result == APP_MOTOR_SAFETY_OK ? APP_MOTOR_CONTROL_OK : APP_MOTOR_CONTROL_INVALID_STATE;
}

app_motor_control_result_t app_motor_control_core_tick(app_motor_control_core_t *core,
                                                        uint32_t now_ms)
{
    if (!core || !core->safety) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    return app_motor_safety_tick(core->safety, now_ms, &core->snapshot) == APP_MOTOR_SAFETY_OK ?
           APP_MOTOR_CONTROL_OK : APP_MOTOR_CONTROL_INVALID_STATE;
}

app_motor_control_result_t app_motor_control_core_submit(
    app_motor_control_core_t *core, const app_motor_safety_command_t *command,
    uint32_t received_ms)
{
    if (!core || !core->safety || !command) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    return app_motor_safety_submit(core->safety, command, received_ms) == APP_MOTOR_SAFETY_OK ?
           APP_MOTOR_CONTROL_OK : APP_MOTOR_CONTROL_INVALID_STATE;
}

void app_motor_control_core_deinit(app_motor_control_core_t *core)
{
    if (core && core->safety) {
        (void)app_motor_safety_disarm(core->safety);
        (void)app_motor_safety_tick(core->safety, 0, &core->snapshot);
        (void)app_motor_safety_destroy(&core->safety);
    }
}
