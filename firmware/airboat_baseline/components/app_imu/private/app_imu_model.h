// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_imu_types.h"

void app_imu_model_init(app_imu_snapshot_t *state);
bool app_imu_model_sample(app_imu_snapshot_t *state, const int16_t accel[3],
                          const int16_t gyro[3], uint64_t timestamp_us);
void app_imu_model_error(app_imu_snapshot_t *state, int32_t error);
void app_imu_model_read(const app_imu_snapshot_t *state, uint64_t now_us,
                        app_imu_snapshot_t *snapshot);
