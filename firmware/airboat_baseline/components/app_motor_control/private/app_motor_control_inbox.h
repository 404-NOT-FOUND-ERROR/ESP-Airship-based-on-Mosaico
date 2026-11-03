// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_motor_control.h"

#define APP_MOTOR_CONTROL_QUEUE_CAPACITY 8U

typedef struct {
    bool is_command;
    app_motor_control_action_t action;
    app_motor_safety_command_t command;
    uint32_t received_ms;
} app_motor_control_request_t;

typedef struct {
    app_motor_control_request_t requests[APP_MOTOR_CONTROL_QUEUE_CAPACITY];
    size_t head;
    size_t count;
    bool priority_pending;
    app_motor_control_action_t priority_action;
} app_motor_control_inbox_t;

void app_motor_control_inbox_init(app_motor_control_inbox_t *inbox);
void app_motor_control_inbox_clear(app_motor_control_inbox_t *inbox);
app_motor_control_result_t app_motor_control_inbox_post(
    app_motor_control_inbox_t *inbox, app_motor_control_action_t action);
app_motor_control_result_t app_motor_control_inbox_submit(
    app_motor_control_inbox_t *inbox, const app_motor_safety_command_t *command,
    uint32_t received_ms);
bool app_motor_control_inbox_take_next(app_motor_control_inbox_t *inbox,
                                       app_motor_control_request_t *request,
                                       bool *priority);
bool app_motor_control_inbox_take_priority(app_motor_control_inbox_t *inbox,
                                           app_motor_control_request_t *request);
