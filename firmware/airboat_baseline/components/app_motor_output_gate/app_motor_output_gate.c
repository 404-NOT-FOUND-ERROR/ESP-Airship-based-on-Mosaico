// SPDX-License-Identifier: Apache-2.0
#include "app_motor_output_gate.h"

#include <string.h>

static bool snapshot_outputs_are_zero(const app_motor_safety_snapshot_t *snapshot)
{
    for (size_t index = 0; index < APP_MOTOR_SAFETY_AXIS_COUNT; ++index) {
        if (snapshot->motor[index].input_a_permille != 0U ||
            snapshot->motor[index].input_b_permille != 0U) {
            return false;
        }
    }
    return true;
}

static bool snapshot_duties_are_valid(const app_motor_safety_snapshot_t *snapshot)
{
    for (size_t index = 0; index < APP_MOTOR_SAFETY_AXIS_COUNT; ++index) {
        if (snapshot->motor[index].input_a_permille > APP_MOTOR_SAFETY_DUTY_MAX ||
            snapshot->motor[index].input_b_permille > APP_MOTOR_SAFETY_DUTY_MAX ||
            (snapshot->motor[index].input_a_permille != 0U &&
             snapshot->motor[index].input_b_permille != 0U)) {
            return false;
        }
    }
    return true;
}

app_motor_output_gate_result_t app_motor_output_gate_build(
    const app_motor_safety_snapshot_t *snapshot,
    app_motor_output_gate_plan_t *out_plan)
{
    if (!out_plan) {
        return APP_MOTOR_OUTPUT_GATE_INVALID_ARGUMENT;
    }
    memset(out_plan, 0, sizeof(*out_plan));
    if (!snapshot) {
        return APP_MOTOR_OUTPUT_GATE_INVALID_ARGUMENT;
    }
    if (!snapshot_duties_are_valid(snapshot)) {
        return APP_MOTOR_OUTPUT_GATE_INVALID_SNAPSHOT;
    }

    const bool permitted = snapshot->armed && !snapshot->emergency_stop_latched &&
                           snapshot->command_fresh;
    if (snapshot->command_fresh &&
        (!snapshot->armed || snapshot->emergency_stop_latched ||
         snapshot->command_age_ms >= APP_MOTOR_SAFETY_COMMAND_TIMEOUT_MS)) {
        return APP_MOTOR_OUTPUT_GATE_INVALID_SNAPSHOT;
    }
    if (!permitted && !snapshot_outputs_are_zero(snapshot)) {
        return APP_MOTOR_OUTPUT_GATE_INVALID_SNAPSHOT;
    }
    if (!permitted) {
        return APP_MOTOR_OUTPUT_GATE_OK;
    }

    memcpy(out_plan->motor, snapshot->motor, sizeof(out_plan->motor));
    out_plan->outputs_allowed = true;
    return APP_MOTOR_OUTPUT_GATE_OK;
}
