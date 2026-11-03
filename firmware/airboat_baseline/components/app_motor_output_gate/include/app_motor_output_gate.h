// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_motor_safety.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    app_motor_safety_motor_output_t motor[APP_MOTOR_SAFETY_AXIS_COUNT];
    bool outputs_allowed;
} app_motor_output_gate_plan_t;

typedef enum {
    APP_MOTOR_OUTPUT_GATE_OK = 0,
    APP_MOTOR_OUTPUT_GATE_INVALID_ARGUMENT,
    APP_MOTOR_OUTPUT_GATE_INVALID_SNAPSHOT,
} app_motor_output_gate_result_t;

/** Convert one safety snapshot to a fail-closed, GPIO-independent output plan. */
app_motor_output_gate_result_t app_motor_output_gate_build(
    const app_motor_safety_snapshot_t *snapshot,
    app_motor_output_gate_plan_t *out_plan);

#ifdef __cplusplus
}
#endif
