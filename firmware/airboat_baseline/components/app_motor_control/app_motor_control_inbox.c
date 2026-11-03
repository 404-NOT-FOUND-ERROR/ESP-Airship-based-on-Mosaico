// SPDX-License-Identifier: Apache-2.0
#include "app_motor_control_inbox.h"

#include <string.h>

void app_motor_control_inbox_init(app_motor_control_inbox_t *inbox)
{
    if (inbox) {
        memset(inbox, 0, sizeof(*inbox));
    }
}

void app_motor_control_inbox_clear(app_motor_control_inbox_t *inbox)
{
    if (inbox) {
        inbox->head = 0;
        inbox->count = 0;
        inbox->priority_pending = false;
    }
}

app_motor_control_result_t app_motor_control_inbox_post(
    app_motor_control_inbox_t *inbox, app_motor_control_action_t action)
{
    if (!inbox || action < APP_MOTOR_CONTROL_ACTION_ARM ||
        action > APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    if (action == APP_MOTOR_CONTROL_ACTION_ESTOP ||
        action == APP_MOTOR_CONTROL_ACTION_DISARM) {
        inbox->head = 0;
        inbox->count = 0;
        if (!inbox->priority_pending || action == APP_MOTOR_CONTROL_ACTION_ESTOP ||
            inbox->priority_action != APP_MOTOR_CONTROL_ACTION_ESTOP) {
            inbox->priority_action = action;
            inbox->priority_pending = true;
        }
        return APP_MOTOR_CONTROL_OK;
    }
    if (inbox->count == APP_MOTOR_CONTROL_QUEUE_CAPACITY) {
        return APP_MOTOR_CONTROL_QUEUE_FULL;
    }
    const size_t tail = (inbox->head + inbox->count) % APP_MOTOR_CONTROL_QUEUE_CAPACITY;
    inbox->requests[tail] = (app_motor_control_request_t) {.action = action};
    ++inbox->count;
    return APP_MOTOR_CONTROL_OK;
}

app_motor_control_result_t app_motor_control_inbox_submit(
    app_motor_control_inbox_t *inbox, const app_motor_safety_command_t *command,
    uint32_t received_ms)
{
    if (!inbox || !command) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    if (inbox->count == APP_MOTOR_CONTROL_QUEUE_CAPACITY) {
        return APP_MOTOR_CONTROL_QUEUE_FULL;
    }
    const size_t tail = (inbox->head + inbox->count) % APP_MOTOR_CONTROL_QUEUE_CAPACITY;
    inbox->requests[tail] = (app_motor_control_request_t) {
        .is_command = true,
        .command = *command,
        .received_ms = received_ms,
    };
    ++inbox->count;
    return APP_MOTOR_CONTROL_OK;
}

bool app_motor_control_inbox_take_priority(app_motor_control_inbox_t *inbox,
                                           app_motor_control_request_t *request)
{
    if (!inbox || !request || !inbox->priority_pending) {
        return false;
    }
    *request = (app_motor_control_request_t) {
        .action = inbox->priority_action,
    };
    inbox->priority_pending = false;
    return true;
}

bool app_motor_control_inbox_take_next(app_motor_control_inbox_t *inbox,
                                       app_motor_control_request_t *request,
                                       bool *priority)
{
    if (!inbox || !request || !priority) {
        return false;
    }
    if (app_motor_control_inbox_take_priority(inbox, request)) {
        *priority = true;
        return true;
    }
    if (inbox->count == 0) {
        return false;
    }
    *request = inbox->requests[inbox->head];
    inbox->head = (inbox->head + 1U) % APP_MOTOR_CONTROL_QUEUE_CAPACITY;
    --inbox->count;
    *priority = false;
    return true;
}
