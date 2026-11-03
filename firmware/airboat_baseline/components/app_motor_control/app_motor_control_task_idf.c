// SPDX-License-Identifier: Apache-2.0
#include "app_motor_control.h"
#include "app_motor_control_inbox.h"

#include <stdlib.h>

#include "app_motor_control_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define CONTROL_TASK_STACK_BYTES 3072U
#define CONTROL_TASK_PRIORITY 4U
#define CONTROL_PERIOD_MS 10U
#define CONTROL_STOP_TIMEOUT_MS 1000U

struct app_motor_control {
    app_motor_control_core_t core;
    app_motor_control_inbox_t inbox;
    SemaphoreHandle_t finished_signal;
    SemaphoreHandle_t lifecycle_lock;
    portMUX_TYPE snapshot_lock;
    app_motor_safety_snapshot_t published;
    bool stop_requested;
};

static uint32_t control_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool should_stop(struct app_motor_control *state)
{
    (void)xSemaphoreTake(state->lifecycle_lock, portMAX_DELAY);
    const bool stop = state->stop_requested;
    (void)xSemaphoreGive(state->lifecycle_lock);
    return stop;
}

static void publish_snapshot(struct app_motor_control *state)
{
    portENTER_CRITICAL(&state->snapshot_lock);
    state->published = state->core.snapshot;
    portEXIT_CRITICAL(&state->snapshot_lock);
}

static bool take_next_request(struct app_motor_control *state,
                              app_motor_control_request_t *request, bool *priority)
{
    (void)xSemaphoreTake(state->lifecycle_lock, portMAX_DELAY);
    const bool found = app_motor_control_inbox_take_next(&state->inbox, request, priority);
    (void)xSemaphoreGive(state->lifecycle_lock);
    return found;
}

static bool take_priority_request(struct app_motor_control *state,
                                  app_motor_control_request_t *request)
{
    (void)xSemaphoreTake(state->lifecycle_lock, portMAX_DELAY);
    const bool found = app_motor_control_inbox_take_priority(&state->inbox, request);
    (void)xSemaphoreGive(state->lifecycle_lock);
    return found;
}

static void apply_request(struct app_motor_control *state,
                          const app_motor_control_request_t *request)
{
    if (!request->is_command) {
        (void)app_motor_control_core_apply(&state->core, request->action);
    } else if (control_now_ms() - request->received_ms >=
               APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS) {
        (void)app_motor_control_core_apply(&state->core, APP_MOTOR_CONTROL_ACTION_DISARM);
    } else {
        (void)app_motor_control_core_submit(&state->core, &request->command,
                                            request->received_ms);
    }
}

static void control_task(void *argument)
{
    struct app_motor_control *state = argument;
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS) > 0 ?
                              pdMS_TO_TICKS(CONTROL_PERIOD_MS) : 1;
    for (;;) {
        if (should_stop(state)) {
            (void)app_motor_control_core_apply(&state->core, APP_MOTOR_CONTROL_ACTION_DISARM);
            (void)app_motor_control_core_tick(&state->core, control_now_ms());
            publish_snapshot(state);
            break;
        }
        app_motor_control_request_t request;
        bool priority;
        size_t processed = 0;
        while (processed < APP_MOTOR_CONTROL_QUEUE_CAPACITY &&
               take_next_request(state, &request, &priority)) {
            apply_request(state, &request);
            ++processed;
            if (priority) {
                break;
            }
        }
        if (take_priority_request(state, &request)) {
            apply_request(state, &request);
        }
        (void)app_motor_control_core_tick(&state->core, control_now_ms());
        publish_snapshot(state);
        vTaskDelayUntil(&last_wake, period);
    }
    xSemaphoreGive(state->finished_signal);
    vTaskDelete(NULL);
}

app_motor_control_result_t app_motor_control_create(app_motor_control_handle_t *out_handle)
{
    if (!out_handle) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    *out_handle = NULL;
    struct app_motor_control *state = calloc(1, sizeof(*state));
    if (!state) {
        return APP_MOTOR_CONTROL_NO_MEMORY;
    }
    state->snapshot_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    state->lifecycle_lock = xSemaphoreCreateMutex();
    state->finished_signal = xSemaphoreCreateBinary();
    app_motor_control_inbox_init(&state->inbox);
    if (!state->lifecycle_lock || !state->finished_signal ||
        app_motor_control_core_init(&state->core, control_now_ms()) != APP_MOTOR_CONTROL_OK) {
        if (state->finished_signal) vSemaphoreDelete(state->finished_signal);
        if (state->lifecycle_lock) vSemaphoreDelete(state->lifecycle_lock);
        free(state);
        return APP_MOTOR_CONTROL_NO_MEMORY;
    }
    state->published = state->core.snapshot;
    if (xTaskCreate(control_task, "airboat_motor", CONTROL_TASK_STACK_BYTES, state,
                    CONTROL_TASK_PRIORITY, NULL) != pdPASS) {
        app_motor_control_core_deinit(&state->core);
        vSemaphoreDelete(state->finished_signal);
        vSemaphoreDelete(state->lifecycle_lock);
        free(state);
        return APP_MOTOR_CONTROL_NO_MEMORY;
    }
    *out_handle = state;
    return APP_MOTOR_CONTROL_OK;
}

app_motor_control_result_t app_motor_control_post(
    app_motor_control_handle_t handle, app_motor_control_action_t action)
{
    if (!handle || action < APP_MOTOR_CONTROL_ACTION_ARM ||
        action > APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    (void)xSemaphoreTake(handle->lifecycle_lock, portMAX_DELAY);
    if (handle->stop_requested) {
        (void)xSemaphoreGive(handle->lifecycle_lock);
        return APP_MOTOR_CONTROL_INVALID_STATE;
    }
    const app_motor_control_result_t result =
        app_motor_control_inbox_post(&handle->inbox, action);
    (void)xSemaphoreGive(handle->lifecycle_lock);
    return result;
}

app_motor_control_result_t app_motor_control_submit(
    app_motor_control_handle_t handle, const app_motor_safety_command_t *command)
{
    if (!handle || !command) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    (void)xSemaphoreTake(handle->lifecycle_lock, portMAX_DELAY);
    if (handle->stop_requested) {
        (void)xSemaphoreGive(handle->lifecycle_lock);
        return APP_MOTOR_CONTROL_INVALID_STATE;
    }
    const app_motor_control_result_t result = app_motor_control_inbox_submit(
        &handle->inbox, command, control_now_ms());
    (void)xSemaphoreGive(handle->lifecycle_lock);
    return result;
}

app_motor_control_result_t app_motor_control_read(
    app_motor_control_handle_t handle, app_motor_safety_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    portENTER_CRITICAL(&handle->snapshot_lock);
    *out_snapshot = handle->published;
    portEXIT_CRITICAL(&handle->snapshot_lock);
    return APP_MOTOR_CONTROL_OK;
}

app_motor_control_result_t app_motor_control_destroy(app_motor_control_handle_t *handle)
{
    if (!handle) return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    struct app_motor_control *state = *handle;
    if (!state) return APP_MOTOR_CONTROL_OK;
    (void)xSemaphoreTake(state->lifecycle_lock, portMAX_DELAY);
    state->stop_requested = true;
    app_motor_control_inbox_clear(&state->inbox);
    (void)xSemaphoreGive(state->lifecycle_lock);
    if (xSemaphoreTake(state->finished_signal,
                       pdMS_TO_TICKS(CONTROL_STOP_TIMEOUT_MS)) != pdTRUE) {
        return APP_MOTOR_CONTROL_TIMEOUT;
    }
    vSemaphoreDelete(state->finished_signal);
    vSemaphoreDelete(state->lifecycle_lock);
    app_motor_control_core_deinit(&state->core);
    free(state);
    *handle = NULL;
    return APP_MOTOR_CONTROL_OK;
}
