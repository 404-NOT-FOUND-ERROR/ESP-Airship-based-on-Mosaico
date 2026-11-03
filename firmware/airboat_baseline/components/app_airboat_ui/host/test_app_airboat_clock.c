// SPDX-License-Identifier: Apache-2.0

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "app_airboat_clock.h"
#include "app_motor_safety.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void assert_stopped(const app_motor_safety_snapshot_t *snapshot)
{
    for (size_t i = 0; i < APP_MOTOR_SAFETY_AXIS_COUNT; ++i) {
        assert(snapshot->motor[i].input_a_permille == 0);
        assert(snapshot->motor[i].input_b_permille == 0);
    }
}

int main(void)
{
    app_motor_safety_handle_t safety = NULL;
    assert(app_motor_safety_create(&safety) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_arm(safety) == APP_MOTOR_SAFETY_OK);

    const app_motor_safety_command_t command = {.axis = {400, 0, 0}};
    const uint32_t submitted_at_ms = app_airboat_clock_now_ms();
    assert(app_motor_safety_submit(safety, &command, submitted_at_ms) ==
           APP_MOTOR_SAFETY_OK);

    app_motor_safety_snapshot_t snapshot;
    assert(app_motor_safety_tick(safety, submitted_at_ms, &snapshot) ==
           APP_MOTOR_SAFETY_OK);
    assert(snapshot.command_fresh);
    assert(snapshot.motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille == 400);

    Sleep(APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS + 50U);
    const uint32_t resumed_at_ms = app_airboat_clock_now_ms();
    assert((uint32_t)(resumed_at_ms - submitted_at_ms) >
           APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS);
    assert(app_motor_safety_tick(safety, resumed_at_ms, &snapshot) ==
           APP_MOTOR_SAFETY_OK);
    assert(!snapshot.command_fresh);
    assert(!snapshot.armed);
    assert_stopped(&snapshot);
    assert(app_motor_safety_destroy(&safety) == APP_MOTOR_SAFETY_OK);

    puts("app_airboat_clock: delayed tick expires command and zeros output");
    return 0;
}
