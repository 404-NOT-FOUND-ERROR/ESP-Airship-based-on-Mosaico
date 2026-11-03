// SPDX-License-Identifier: Apache-2.0
#include "app_imu_model.h"

static uint32_t increment(uint32_t value)
{
    return value == UINT32_MAX ? value : value + 1U;
}

void app_imu_model_init(app_imu_snapshot_t *state)
{
    *state = (app_imu_snapshot_t) {
        .state = APP_IMU_STARTING,
        .age_ms = UINT32_MAX,
    };
}

bool app_imu_model_sample(app_imu_snapshot_t *state, const int16_t accel[3],
                          const int16_t gyro[3], uint64_t timestamp_us)
{
    if (state->sample_count && timestamp_us <= state->timestamp_us) {
        return false;
    }
    for (unsigned axis = 0; axis < 3; ++axis) {
        state->accel_g[axis] = (float)accel[axis] * (APP_IMU_ACCEL_RANGE_G / 32768.0f);
        state->gyro_dps[axis] = (float)gyro[axis] * (APP_IMU_GYRO_RANGE_DPS / 32768.0f);
    }
    state->sample_count = increment(state->sample_count);
    state->timestamp_us = timestamp_us;
    state->age_ms = 0;
    state->state = APP_IMU_RUNNING;
    state->consecutive_errors = 0;
    state->last_error = 0;
    state->valid = true;
    return true;
}

void app_imu_model_error(app_imu_snapshot_t *state, int32_t error)
{
    state->error_count = increment(state->error_count);
    state->consecutive_errors = increment(state->consecutive_errors);
    state->last_error = error;
    state->valid = false;
    state->state = state->consecutive_errors >= APP_IMU_FAULT_THRESHOLD ?
                   APP_IMU_FAULTED : APP_IMU_DEGRADED;
}

void app_imu_model_read(const app_imu_snapshot_t *state, uint64_t now_us,
                        app_imu_snapshot_t *snapshot)
{
    *snapshot = *state;
    if (!state->sample_count || now_us < state->timestamp_us) {
        snapshot->age_ms = UINT32_MAX;
        snapshot->valid = false;
        return;
    }
    const uint64_t elapsed_us = now_us - state->timestamp_us;
    const uint64_t age_ms = elapsed_us / 1000U;
    snapshot->age_ms = age_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)age_ms;
    snapshot->valid = state->valid && state->state == APP_IMU_RUNNING &&
                      elapsed_us <= (uint64_t)APP_IMU_STALE_MS * 1000U;
}
