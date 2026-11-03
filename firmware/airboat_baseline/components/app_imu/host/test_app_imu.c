// SPDX-License-Identifier: Apache-2.0
#include "app_imu_model.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const int16_t level_accel[3] = {0, 0, 8192};
static const int16_t stopped_gyro[3] = {0, 0, 0};

static void test_units_and_signs(void)
{
    app_imu_snapshot_t state;
    app_imu_model_init(&state);
    const int16_t accel[3] = {-32768, 8192, 32767};
    const int16_t gyro[3] = {-32768, 16384, 32767};
    assert(app_imu_model_sample(&state, accel, gyro, 1000000));
    assert(state.accel_g[0] == -4.0f && state.accel_g[1] == 1.0f);
    assert(fabsf(state.accel_g[2] - 3.999878f) < 0.00001f);
    assert(state.gyro_dps[0] == -500.0f && state.gyro_dps[1] == 250.0f);
    assert(fabsf(state.gyro_dps[2] - 499.98474f) < 0.001f);
    assert(state.valid && state.state == APP_IMU_RUNNING);
}

static void test_freshness_and_clock(void)
{
    app_imu_snapshot_t state, copy;
    app_imu_model_init(&state);
    app_imu_model_read(&state, 0, &copy);
    assert(!copy.valid && copy.age_ms == UINT32_MAX);
    const uint64_t start = (uint64_t)UINT32_MAX * 1000U;
    assert(app_imu_model_sample(&state, level_accel, stopped_gyro, start));
    app_imu_model_read(&state, start + 100000U, &copy);
    assert(copy.valid && copy.age_ms == APP_IMU_STALE_MS);
    app_imu_model_read(&state, start + 100001U, &copy);
    assert(!copy.valid);
    assert(state.valid); /* A read never changes published state. */
    app_imu_model_read(&state, start - 1U, &copy);
    assert(!copy.valid && copy.age_ms == UINT32_MAX);
    app_imu_model_read(&state, start + ((uint64_t)UINT32_MAX + 1U) * 1000U, &copy);
    assert(!copy.valid && copy.age_ms == UINT32_MAX);
}

static void test_fault_and_recovery(void)
{
    app_imu_snapshot_t state, copy;
    app_imu_model_init(&state);
    assert(app_imu_model_sample(&state, level_accel, stopped_gyro, 1000));
    app_imu_model_error(&state, 0x107);
    app_imu_model_read(&state, 2000, &copy);
    assert(!copy.valid && copy.state == APP_IMU_DEGRADED);
    assert(copy.timestamp_us == 1000 && copy.accel_g[2] == 1.0f);
    for (unsigned i = 1; i < APP_IMU_FAULT_THRESHOLD; ++i) {
        app_imu_model_error(&state, 0x108);
    }
    assert(state.state == APP_IMU_FAULTED && state.last_error == 0x108);
    assert(app_imu_model_sample(&state, level_accel, stopped_gyro, 3000));
    assert(state.valid && state.state == APP_IMU_RUNNING && state.last_error == 0);
    assert(state.error_count == APP_IMU_FAULT_THRESHOLD && state.consecutive_errors == 0);
    state.state = APP_IMU_STOPPED;
    app_imu_model_read(&state, 3001, &copy);
    assert(!copy.valid);
}

static void test_monotonic_samples_and_saturation(void)
{
    app_imu_snapshot_t state, previous;
    app_imu_model_init(&state);
    assert(app_imu_model_sample(&state, level_accel, stopped_gyro, 0));
    previous = state;
    assert(!app_imu_model_sample(&state, level_accel, stopped_gyro, 0));
    assert(memcmp(&state, &previous, sizeof(state)) == 0);
    assert(app_imu_model_sample(&state, level_accel, stopped_gyro, 5000));
    assert(!app_imu_model_sample(&state, level_accel, stopped_gyro, 4999));
    state.sample_count = UINT32_MAX;
    assert(app_imu_model_sample(&state, level_accel, stopped_gyro, 6000));
    assert(state.sample_count == UINT32_MAX);
    state.error_count = UINT32_MAX;
    state.consecutive_errors = UINT32_MAX;
    app_imu_model_error(&state, 0x107);
    assert(state.error_count == UINT32_MAX && state.consecutive_errors == UINT32_MAX);
}

int main(void)
{
    test_units_and_signs();
    test_freshness_and_clock();
    test_fault_and_recovery();
    test_monotonic_samples_and_saturation();
    puts("BMI270 model: 4 test groups passed (units, freshness, faults, counters)");
    return 0;
}
