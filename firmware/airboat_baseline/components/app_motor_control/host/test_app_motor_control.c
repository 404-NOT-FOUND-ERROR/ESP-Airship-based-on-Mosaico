// SPDX-License-Identifier: Apache-2.0
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <assert.h>
#include <stdio.h>

#include "app_motor_control.h"
#include "app_motor_control_inbox.h"

static app_motor_safety_snapshot_t read_snapshot(app_motor_control_handle_t handle)
{
    app_motor_safety_snapshot_t snapshot;
    assert(app_motor_control_read(handle, &snapshot) == APP_MOTOR_CONTROL_OK);
    return snapshot;
}

static bool wait_for(app_motor_control_handle_t handle, bool armed, bool estop)
{
    const ULONGLONG deadline = GetTickCount64() + 500U;
    do {
        const app_motor_safety_snapshot_t snapshot = read_snapshot(handle);
        if (snapshot.armed == armed && snapshot.emergency_stop_latched == estop) {
            return true;
        }
        Sleep(2);
    } while (GetTickCount64() < deadline);
    return false;
}

static bool wait_for_output(app_motor_control_handle_t handle, uint16_t input_a,
                            uint32_t timeout_ms)
{
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    do {
        const app_motor_safety_snapshot_t snapshot = read_snapshot(handle);
        if (snapshot.command_fresh && snapshot.motor[0].input_a_permille == input_a) {
            return true;
        }
        Sleep(2);
    } while (GetTickCount64() < deadline);
    return false;
}

static void assert_outputs_zero(const app_motor_safety_snapshot_t *snapshot)
{
    for (size_t index = 0; index < APP_MOTOR_SAFETY_AXIS_COUNT; ++index) {
        assert(snapshot->motor[index].input_a_permille == 0);
        assert(snapshot->motor[index].input_b_permille == 0);
    }
}

static void test_startup_and_action_queue(void)
{
    app_motor_control_handle_t control = NULL;
    assert(app_motor_control_create(&control) == APP_MOTOR_CONTROL_OK);
    app_motor_safety_snapshot_t snapshot = read_snapshot(control);
    assert(!snapshot.armed && !snapshot.emergency_stop_latched);
    assert_outputs_zero(&snapshot);

    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_ARM) == APP_MOTOR_CONTROL_OK);
    assert(wait_for(control, true, false));
    snapshot = read_snapshot(control);
    assert(!snapshot.command_fresh);
    assert_outputs_zero(&snapshot);

    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_DISARM) == APP_MOTOR_CONTROL_OK);
    assert(wait_for(control, false, false));
    snapshot = read_snapshot(control);
    assert_outputs_zero(&snapshot);
    assert(app_motor_control_destroy(&control) == APP_MOTOR_CONTROL_OK);
    puts("motor control: startup inhibit and asynchronous action queue passed");
}

static void test_estop_priority_and_clear_sequence(void)
{
    app_motor_control_handle_t control = NULL;
    assert(app_motor_control_create(&control) == APP_MOTOR_CONTROL_OK);
    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_ESTOP) == APP_MOTOR_CONTROL_OK);
    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_DISARM) == APP_MOTOR_CONTROL_OK);
    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_ARM) == APP_MOTOR_CONTROL_OK);
    assert(wait_for(control, false, true));
    app_motor_safety_snapshot_t snapshot = read_snapshot(control);
    assert_outputs_zero(&snapshot);

    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP) == APP_MOTOR_CONTROL_OK);
    assert(wait_for(control, false, false));
    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_ARM) == APP_MOTOR_CONTROL_OK);
    assert(wait_for(control, true, false));
    snapshot = read_snapshot(control);
    assert_outputs_zero(&snapshot);
    assert(app_motor_control_destroy(&control) == APP_MOTOR_CONTROL_OK);
    puts("motor control: E-STOP priority, explicit clear and re-arm passed");
}

static void test_timestamped_command_watchdog(void)
{
    app_motor_control_handle_t control = NULL;
    assert(app_motor_control_create(&control) == APP_MOTOR_CONTROL_OK);
    assert(app_motor_control_post(control, APP_MOTOR_CONTROL_ACTION_ARM) == APP_MOTOR_CONTROL_OK);
    assert(wait_for(control, true, false));

    const app_motor_safety_command_t command = {.axis = {400, 0, 0}};
    assert(app_motor_control_submit(control, &command) == APP_MOTOR_CONTROL_OK);
    assert(wait_for_output(control, 400, 200));
    app_motor_safety_snapshot_t snapshot = read_snapshot(control);
    assert(snapshot.motor[0].input_b_permille == 0);
    assert(snapshot.motor[1].input_a_permille == 0);
    assert(snapshot.motor[1].input_b_permille == 0);
    assert(snapshot.motor[2].input_a_permille == 0);
    assert(snapshot.motor[2].input_b_permille == 0);

    assert(wait_for(control, false, false));
    snapshot = read_snapshot(control);
    assert(!snapshot.command_fresh);
    assert_outputs_zero(&snapshot);
    assert(app_motor_control_destroy(&control) == APP_MOTOR_CONTROL_OK);
    puts("motor control: owner-timestamped command and stale-command DISARM passed");
}

static void test_inbox_capacity_and_priority_order(void)
{
    app_motor_control_inbox_t inbox;
    app_motor_control_inbox_init(&inbox);
    for (size_t index = 0; index < APP_MOTOR_CONTROL_QUEUE_CAPACITY; ++index) {
        assert(app_motor_control_inbox_post(&inbox, APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP) ==
               APP_MOTOR_CONTROL_OK);
    }
    assert(app_motor_control_inbox_post(&inbox, APP_MOTOR_CONTROL_ACTION_CLEAR_ESTOP) ==
           APP_MOTOR_CONTROL_QUEUE_FULL);
    const app_motor_safety_command_t command = {.axis = {300, 0, 0}};
    assert(app_motor_control_inbox_submit(&inbox, &command, 100U) ==
           APP_MOTOR_CONTROL_QUEUE_FULL);
    assert(app_motor_control_inbox_post(&inbox, APP_MOTOR_CONTROL_ACTION_ESTOP) ==
           APP_MOTOR_CONTROL_OK);
    assert(inbox.count == 0U);
    assert(app_motor_control_inbox_post(&inbox, APP_MOTOR_CONTROL_ACTION_DISARM) ==
           APP_MOTOR_CONTROL_OK);
    assert(app_motor_control_inbox_post(&inbox, APP_MOTOR_CONTROL_ACTION_ARM) ==
           APP_MOTOR_CONTROL_OK);

    app_motor_control_request_t request;
    bool priority = false;
    assert(app_motor_control_inbox_take_next(&inbox, &request, &priority));
    assert(priority && !request.is_command &&
           request.action == APP_MOTOR_CONTROL_ACTION_ESTOP);
    assert(app_motor_control_inbox_take_next(&inbox, &request, &priority));
    assert(!priority && request.action == APP_MOTOR_CONTROL_ACTION_ARM);
    assert(!app_motor_control_inbox_take_next(&inbox, &request, &priority));
    puts("motor control: deterministic inbox capacity, flush and priority order passed");
}

static void test_invalid_inputs_and_teardown(void)
{
    app_motor_control_handle_t control = NULL;
    assert(app_motor_control_create(NULL) == APP_MOTOR_CONTROL_INVALID_ARGUMENT);
    assert(app_motor_control_create(&control) == APP_MOTOR_CONTROL_OK);
    assert(app_motor_control_post(control, (app_motor_control_action_t)99) ==
           APP_MOTOR_CONTROL_INVALID_ARGUMENT);
    assert(app_motor_control_submit(control, NULL) == APP_MOTOR_CONTROL_INVALID_ARGUMENT);
    assert(app_motor_control_read(control, NULL) == APP_MOTOR_CONTROL_INVALID_ARGUMENT);
    assert(app_motor_control_destroy(&control) == APP_MOTOR_CONTROL_OK);
    assert(control == NULL);
    assert(app_motor_control_destroy(&control) == APP_MOTOR_CONTROL_OK);
    puts("motor control: invalid inputs and bounded teardown passed");
}

int main(void)
{
    test_startup_and_action_queue();
    test_estop_priority_and_clear_sequence();
    test_timestamped_command_watchdog();
    test_inbox_capacity_and_priority_order();
    test_invalid_inputs_and_teardown();
    puts("app_motor_control: 5 test groups passed; no GPIO/PWM adapter exists");
    return 0;
}
