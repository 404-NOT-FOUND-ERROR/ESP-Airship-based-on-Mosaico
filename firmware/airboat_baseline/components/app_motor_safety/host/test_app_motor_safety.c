// SPDX-License-Identifier: Apache-2.0

#include "app_motor_safety.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void assert_stopped(const app_motor_safety_snapshot_t *snapshot)
{
    for (size_t i = 0; i < APP_MOTOR_SAFETY_AXIS_COUNT; ++i) {
        assert(snapshot->motor[i].input_a_permille == 0);
        assert(snapshot->motor[i].input_b_permille == 0);
    }
}

static app_motor_safety_snapshot_t tick(app_motor_safety_handle_t handle, uint32_t now_ms)
{
    app_motor_safety_snapshot_t snapshot;
    assert(app_motor_safety_tick(handle, now_ms, &snapshot) == APP_MOTOR_SAFETY_OK);
    return snapshot;
}

static void test_starts_inhibited_and_requires_fresh_command(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_snapshot_t snapshot = tick(handle, 10);
    assert(!snapshot.armed);
    assert_stopped(&snapshot);

    app_motor_safety_command_t command = {.axis = {500, -300, 900}};
    assert(app_motor_safety_submit(handle, &command, 11) == APP_MOTOR_SAFETY_INVALID_STATE);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 12);
    assert(snapshot.armed && !snapshot.command_fresh);
    assert_stopped(&snapshot);

    assert(app_motor_safety_submit(handle, &command, 13) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 13);
    assert(snapshot.command_fresh);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille == 500);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 0);
    assert(snapshot.motor[APP_MOTOR_SAFETY_YAW].input_a_permille == 0);
    assert(snapshot.motor[APP_MOTOR_SAFETY_YAW].input_b_permille == 300);
    assert(snapshot.motor[APP_MOTOR_SAFETY_LIFT].input_a_permille == 900);
    assert(snapshot.motor[APP_MOTOR_SAFETY_LIFT].input_b_permille == 0);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
    assert(handle == NULL);
}

static void test_clamps_and_inserts_reversal_deadtime(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);

    app_motor_safety_command_t command = {.axis = {INT16_MAX, 0, 0}};
    assert(app_motor_safety_submit(handle, &command, 100) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_snapshot_t snapshot = tick(handle, 100);
    assert(snapshot.motor[0].input_a_permille == APP_MOTOR_SAFETY_DUTY_MAX);

    command.axis[0] = -600;
    assert(app_motor_safety_submit(handle, &command, 110) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 110);
    assert(snapshot.motor[0].input_a_permille == 0);
    assert(snapshot.motor[0].input_b_permille == 0);
    snapshot = tick(handle, 159);
    assert(snapshot.motor[0].input_b_permille == 0);
    snapshot = tick(handle, 160);
    assert(snapshot.motor[0].input_a_permille == 0);
    assert(snapshot.motor[0].input_b_permille == 600);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
}

static void test_timeout_and_uint32_wraparound(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_command_t command = {.axis = {700, 0, 0}};
    const uint32_t submitted_at = UINT32_MAX - 100U;
    assert(app_motor_safety_submit(handle, &command, submitted_at) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_snapshot_t snapshot = tick(handle, 148U);
    assert(snapshot.command_fresh);
    assert(snapshot.motor[0].input_a_permille == 700);
    snapshot = tick(handle, 149U);
    assert(!snapshot.command_fresh);
    assert(!snapshot.armed);
    assert_stopped(&snapshot);
    assert(app_motor_safety_submit(handle, &command, 150U) ==
           APP_MOTOR_SAFETY_INVALID_STATE);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 151U);
    assert(!snapshot.command_fresh);
    assert_stopped(&snapshot);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
}

static void test_estop_requires_disarmed_clear_and_never_restores_command(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_clear_emergency_stop(handle) == APP_MOTOR_SAFETY_INVALID_STATE);
    app_motor_safety_command_t command = {.axis = {400, 300, -200}};
    assert(app_motor_safety_submit(handle, &command, 20) == APP_MOTOR_SAFETY_OK);
    (void)tick(handle, 20);

    assert(app_motor_safety_latch_emergency_stop(handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_INVALID_STATE);
    app_motor_safety_snapshot_t snapshot = tick(handle, 21);
    assert(!snapshot.armed && snapshot.emergency_stop_latched);
    assert_stopped(&snapshot);
    assert(app_motor_safety_clear_emergency_stop(handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 22);
    assert(!snapshot.command_fresh);
    assert_stopped(&snapshot);

    command.axis[APP_MOTOR_SAFETY_PROPULSION] = -400;
    assert(app_motor_safety_submit(handle, &command, 23) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 23);
    assert_stopped(&snapshot);
    snapshot = tick(handle, 71);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 0);
    snapshot = tick(handle, 72);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille == 0);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 400);

    assert(app_motor_safety_latch_emergency_stop(handle) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 74);
    assert_stopped(&snapshot);
    assert(app_motor_safety_clear_emergency_stop(handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
}

static void test_disarm_clears_internal_output_before_rearm(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);

    app_motor_safety_command_t command = {.axis = {500, 0, 0}};
    assert(app_motor_safety_submit(handle, &command, 10) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_snapshot_t snapshot = tick(handle, 10);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille == 500);

    assert(app_motor_safety_disarm(handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 11);
    assert(!snapshot.command_fresh);
    assert_stopped(&snapshot);

    assert(app_motor_safety_submit(handle, &command, 12) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 12);
    assert_stopped(&snapshot);
    snapshot = tick(handle, 61);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille == 500);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 0);

    command.axis[APP_MOTOR_SAFETY_PROPULSION] = -500;
    assert(app_motor_safety_submit(handle, &command, 70) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 70);
    assert_stopped(&snapshot);
    snapshot = tick(handle, 120);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 500);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
}

static void test_reversal_deadtime_across_uint32_wraparound(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);

    app_motor_safety_command_t command = {.axis = {300, 0, 0}};
    assert(app_motor_safety_submit(handle, &command, UINT32_MAX - 20U) ==
           APP_MOTOR_SAFETY_OK);
    app_motor_safety_snapshot_t snapshot = tick(handle, UINT32_MAX - 20U);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille == 300);

    command.axis[APP_MOTOR_SAFETY_PROPULSION] = -300;
    assert(app_motor_safety_submit(handle, &command, UINT32_MAX - 10U) ==
           APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, UINT32_MAX - 10U);
    assert_stopped(&snapshot);
    snapshot = tick(handle, 38U);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 0);
    snapshot = tick(handle, 39U);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille == 300);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
}

static void test_late_packet_cannot_revive_session_before_tick(void)
{
    app_motor_safety_handle_t handle = NULL;
    assert(app_motor_safety_create(&handle) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_command_t command = {.axis = {300, 200, -100}};
    assert(app_motor_safety_submit(handle, &command, 100U) == APP_MOTOR_SAFETY_OK);
    (void)tick(handle, 100U);

    assert(app_motor_safety_submit(handle, &command, 350U) ==
           APP_MOTOR_SAFETY_INVALID_STATE);
    app_motor_safety_snapshot_t snapshot = tick(handle, 350U);
    assert(!snapshot.armed && !snapshot.command_fresh);
    assert_stopped(&snapshot);
    assert(app_motor_safety_submit(handle, &command, 351U) ==
           APP_MOTOR_SAFETY_INVALID_STATE);

    assert(app_motor_safety_arm(handle) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 352U);
    assert_stopped(&snapshot);
    assert(app_motor_safety_submit(handle, &command, 353U) == APP_MOTOR_SAFETY_OK);
    snapshot = tick(handle, 402U);
    assert(snapshot.armed && snapshot.command_fresh);
    assert(snapshot.motor[0].input_a_permille == 300);
    assert(app_motor_safety_destroy(&handle) == APP_MOTOR_SAFETY_OK);
}

int main(void)
{
    test_starts_inhibited_and_requires_fresh_command();
    test_clamps_and_inserts_reversal_deadtime();
    test_timeout_and_uint32_wraparound();
    test_estop_requires_disarmed_clear_and_never_restores_command();
    test_disarm_clears_internal_output_before_rearm();
    test_reversal_deadtime_across_uint32_wraparound();
    test_late_packet_cannot_revive_session_before_tick();
    puts("app_motor_safety: 7 test groups passed");
    return 0;
}
