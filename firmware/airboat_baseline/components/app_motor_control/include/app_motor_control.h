// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_motor_safety.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app_motor_control *app_motor_control_handle_t;

typedef enum {
    APP_MOTOR_CONTROL_ACTION_ARM = 0,
    APP_MOTOR_CONTROL_ACTION_DISARM,
    APP_MOTOR_CONTROL_ACTION_ESTOP,
    APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP,
} app_motor_control_action_t;

typedef enum {
    APP_MOTOR_CONTROL_OK = 0,
    APP_MOTOR_CONTROL_INVALID_ARGUMENT,
    APP_MOTOR_CONTROL_NO_MEMORY,
    APP_MOTOR_CONTROL_QUEUE_FULL,
    APP_MOTOR_CONTROL_INVALID_STATE,
    APP_MOTOR_CONTROL_TIMEOUT,
} app_motor_control_result_t;

/** Start one boot-lifetime owner task with inhibited outputs. */
app_motor_control_result_t app_motor_control_create(app_motor_control_handle_t *out_handle);

/** Queue a UI safety action; E-STOP and DISARM replace pending lower-priority actions. */
app_motor_control_result_t app_motor_control_post(
    app_motor_control_handle_t handle, app_motor_control_action_t action);

/** Queue a bounded motion command with its owner-clock receipt time. */
app_motor_control_result_t app_motor_control_submit(
    app_motor_control_handle_t handle, const app_motor_safety_command_t *command);

/** Copy the latest task-owned safety snapshot. */
app_motor_control_result_t app_motor_control_read(
    app_motor_control_handle_t handle, app_motor_safety_snapshot_t *out_snapshot);

/** Stop the task, force DISARMED and release the handle. */
app_motor_control_result_t app_motor_control_destroy(app_motor_control_handle_t *handle);

#ifdef __cplusplus
}
#endif
