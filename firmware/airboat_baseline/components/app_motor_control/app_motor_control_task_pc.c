// SPDX-License-Identifier: Apache-2.0
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdlib.h>

#include "app_motor_control.h"
#include "app_motor_control_core.h"
#include "app_motor_control_inbox.h"

#define CONTROL_PERIOD_MS 10U
#define CONTROL_STOP_TIMEOUT_MS 1000U

struct app_motor_control {
    app_motor_control_core_t core;
    SRWLOCK lock;
    CONDITION_VARIABLE wake;
    HANDLE task;
    app_motor_control_inbox_t inbox;
    app_motor_safety_snapshot_t published;
    bool stop_requested;
};

static uint32_t control_now_ms(void)
{
    return (uint32_t)GetTickCount64();
}

static bool take_next_request(struct app_motor_control *state,
                              app_motor_control_request_t *request, bool *priority)
{
    bool found = false;
    AcquireSRWLockExclusive(&state->lock);
    found = app_motor_control_inbox_take_next(&state->inbox, request, priority);
    ReleaseSRWLockExclusive(&state->lock);
    return found;
}

static bool take_priority_request(struct app_motor_control *state,
                                  app_motor_control_request_t *request)
{
    AcquireSRWLockExclusive(&state->lock);
    const bool found = app_motor_control_inbox_take_priority(&state->inbox, request);
    ReleaseSRWLockExclusive(&state->lock);
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

static DWORD WINAPI control_task(void *argument)
{
    struct app_motor_control *state = argument;
    for (;;) {
        AcquireSRWLockExclusive(&state->lock);
        const ULONGLONG deadline = GetTickCount64() + CONTROL_PERIOD_MS;
        while (!state->stop_requested) {
            const ULONGLONG now = GetTickCount64();
            if (now >= deadline) {
                break;
            }
            const DWORD remaining = (DWORD)(deadline - now);
            if (!SleepConditionVariableSRW(&state->wake, &state->lock, remaining, 0) &&
                GetLastError() == ERROR_TIMEOUT) {
                break;
            }
        }
        const bool stop = state->stop_requested;
        ReleaseSRWLockExclusive(&state->lock);

        if (stop) {
            (void)app_motor_control_core_apply(&state->core, APP_MOTOR_CONTROL_ACTION_DISARM);
            (void)app_motor_control_core_tick(&state->core, control_now_ms());
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
        if (!stop && take_priority_request(state, &request)) {
            apply_request(state, &request);
        }
        (void)app_motor_control_core_tick(&state->core, control_now_ms());

        AcquireSRWLockExclusive(&state->lock);
        state->published = state->core.snapshot;
        ReleaseSRWLockExclusive(&state->lock);
    }
    AcquireSRWLockExclusive(&state->lock);
    state->published = state->core.snapshot;
    ReleaseSRWLockExclusive(&state->lock);
    return 0;
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
    InitializeSRWLock(&state->lock);
    InitializeConditionVariable(&state->wake);
    app_motor_control_inbox_init(&state->inbox);
    const app_motor_control_result_t initialized =
        app_motor_control_core_init(&state->core, control_now_ms());
    if (initialized != APP_MOTOR_CONTROL_OK) {
        free(state);
        return initialized;
    }
    state->published = state->core.snapshot;
    state->task = CreateThread(NULL, 0, control_task, state, 0, NULL);
    if (!state->task) {
        app_motor_control_core_deinit(&state->core);
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
    AcquireSRWLockExclusive(&handle->lock);
    if (handle->stop_requested) {
        ReleaseSRWLockExclusive(&handle->lock);
        return APP_MOTOR_CONTROL_INVALID_STATE;
    }
    const app_motor_control_result_t result =
        app_motor_control_inbox_post(&handle->inbox, action);
    if (result == APP_MOTOR_CONTROL_OK) {
        WakeConditionVariable(&handle->wake);
    }
    ReleaseSRWLockExclusive(&handle->lock);
    return result;
}

app_motor_control_result_t app_motor_control_submit(
    app_motor_control_handle_t handle, const app_motor_safety_command_t *command)
{
    if (!handle || !command) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    AcquireSRWLockExclusive(&handle->lock);
    if (handle->stop_requested) {
        ReleaseSRWLockExclusive(&handle->lock);
        return APP_MOTOR_CONTROL_INVALID_STATE;
    }
    const app_motor_control_result_t result = app_motor_control_inbox_submit(
        &handle->inbox, command, control_now_ms());
    if (result == APP_MOTOR_CONTROL_OK) WakeConditionVariable(&handle->wake);
    ReleaseSRWLockExclusive(&handle->lock);
    return result;
}

app_motor_control_result_t app_motor_control_read(
    app_motor_control_handle_t handle, app_motor_safety_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    AcquireSRWLockShared(&handle->lock);
    *out_snapshot = handle->published;
    ReleaseSRWLockShared(&handle->lock);
    return APP_MOTOR_CONTROL_OK;
}

app_motor_control_result_t app_motor_control_destroy(app_motor_control_handle_t *handle)
{
    if (!handle) {
        return APP_MOTOR_CONTROL_INVALID_ARGUMENT;
    }
    struct app_motor_control *state = *handle;
    if (!state) {
        return APP_MOTOR_CONTROL_OK;
    }
    AcquireSRWLockExclusive(&state->lock);
    state->stop_requested = true;
    app_motor_control_inbox_clear(&state->inbox);
    WakeAllConditionVariable(&state->wake);
    ReleaseSRWLockExclusive(&state->lock);
    if (WaitForSingleObject(state->task, CONTROL_STOP_TIMEOUT_MS) != WAIT_OBJECT_0) {
        return APP_MOTOR_CONTROL_TIMEOUT;
    }
    CloseHandle(state->task);
    app_motor_control_core_deinit(&state->core);
    free(state);
    *handle = NULL;
    return APP_MOTOR_CONTROL_OK;
}
