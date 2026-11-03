// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>

#include "app_motor_output_gate.h"

static void assert_zero_plan(const app_motor_output_gate_plan_t *plan)
{
    assert(!plan->outputs_allowed);
    for (size_t index = 0; index < APP_MOTOR_SAFETY_AXIS_COUNT; ++index) {
        assert(plan->motor[index].input_a_permille == 0U);
        assert(plan->motor[index].input_b_permille == 0U);
    }
}

static void test_fresh_mapping(void)
{
    const app_motor_safety_snapshot_t snapshot = {
        .motor = {{700U, 0U}, {0U, 300U}, {1000U, 0U}},
        .armed = true,
        .command_fresh = true,
        .command_age_ms = APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS - 1U,
    };
    app_motor_output_gate_plan_t plan;
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert(plan.outputs_allowed);
    assert(plan.motor[0].input_a_permille == 700U && plan.motor[0].input_b_permille == 0U);
    assert(plan.motor[1].input_a_permille == 0U && plan.motor[1].input_b_permille == 300U);
    assert(plan.motor[2].input_a_permille == 1000U && plan.motor[2].input_b_permille == 0U);
}

static void test_safe_states_are_zero(void)
{
    static const app_motor_safety_snapshot_t snapshots[] = {
        {0},
        {.armed = true},
        {.emergency_stop_latched = true},
    };
    for (size_t index = 0; index < sizeof(snapshots) / sizeof(snapshots[0]); ++index) {
        app_motor_output_gate_plan_t plan;
        assert(app_motor_output_gate_build(&snapshots[index], &plan) ==
               APP_MOTOR_OUTPUT_GATE_OK);
        assert_zero_plan(&plan);
    }
}

static void test_inconsistent_snapshots_fail_closed(void)
{
    static const app_motor_safety_snapshot_t invalid[] = {
        {.armed = true, .command_fresh = true,
         .command_age_ms = APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS},
        {.command_fresh = true},
        {.armed = true, .emergency_stop_latched = true, .command_fresh = true},
        {.motor = {{1U, 0U}}},
        {.motor = {{APP_MOTOR_SAFETY_DUTY_MAX + 1U, 0U}},
         .armed = true, .command_fresh = true},
        {.motor = {{1U, 1U}}, .armed = true, .command_fresh = true},
        {.motor = {{0U, 0U}, {0U, APP_MOTOR_SAFETY_DUTY_MAX + 1U}},
         .armed = true, .command_fresh = true},
        {.motor = {{0U, 0U}, {0U, 0U}, {100U, 1U}},
         .armed = true, .command_fresh = true},
    };
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        app_motor_output_gate_plan_t plan = {.outputs_allowed = true};
        assert(app_motor_output_gate_build(&invalid[index], &plan) ==
               APP_MOTOR_OUTPUT_GATE_INVALID_SNAPSHOT);
        assert_zero_plan(&plan);
    }
}

static void test_owner_tick_contract(void)
{
    app_motor_safety_handle_t safety = NULL;
    assert(app_motor_safety_create(&safety) == APP_MOTOR_SAFETY_OK);
    app_motor_safety_snapshot_t snapshot;
    app_motor_output_gate_plan_t plan;
    assert(app_motor_safety_tick(safety, 0U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert_zero_plan(&plan);
    assert(app_motor_safety_arm(safety) == APP_MOTOR_SAFETY_OK);
    const app_motor_safety_command_t command = {.axis = {700, -300, 1000}};
    assert(app_motor_safety_submit(safety, &command, 10U) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_tick(safety, 10U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert(plan.outputs_allowed && plan.motor[0].input_a_permille == 700U);
    const app_motor_safety_command_t reverse = {.axis = {-700, 300, -1000}};
    assert(app_motor_safety_submit(safety, &reverse, 20U) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_tick(safety, 20U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert(plan.outputs_allowed && plan.motor[0].input_a_permille == 0U &&
           plan.motor[0].input_b_permille == 0U);
    assert(app_motor_safety_tick(safety, 70U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert(plan.outputs_allowed && plan.motor[0].input_b_permille == 700U);
    assert(app_motor_safety_tick(safety, 270U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert_zero_plan(&plan);
    assert(!snapshot.armed);
    assert(app_motor_safety_arm(safety) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_submit(safety, &command, 300U) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_tick(safety, 350U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_latch_emergency_stop(safety) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_safety_tick(safety, 351U, &snapshot) == APP_MOTOR_SAFETY_OK);
    assert(app_motor_output_gate_build(&snapshot, &plan) == APP_MOTOR_OUTPUT_GATE_OK);
    assert_zero_plan(&plan);
    assert(app_motor_safety_destroy(&safety) == APP_MOTOR_SAFETY_OK);
}

static void test_invalid_arguments(void)
{
    app_motor_output_gate_plan_t plan = {.outputs_allowed = true};
    assert(app_motor_output_gate_build(NULL, &plan) == APP_MOTOR_OUTPUT_GATE_INVALID_ARGUMENT);
    assert_zero_plan(&plan);
    assert(app_motor_output_gate_build(NULL, NULL) == APP_MOTOR_OUTPUT_GATE_INVALID_ARGUMENT);
}

int main(void)
{
    test_fresh_mapping();
    test_safe_states_are_zero();
    test_inconsistent_snapshots_fail_closed();
    test_invalid_arguments();
    test_owner_tick_contract();
    puts("app_motor_output_gate: 5 test groups passed");
    return 0;
}
