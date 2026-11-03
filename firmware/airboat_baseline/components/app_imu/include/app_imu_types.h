// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APP_IMU_PERIOD_MS 20U
#define APP_IMU_STALE_MS 100U
#define APP_IMU_FAULT_THRESHOLD 5U
#define APP_IMU_ACCEL_RANGE_G 4.0f
#define APP_IMU_GYRO_RANGE_DPS 500.0f

typedef enum {
    APP_IMU_STARTING,
    APP_IMU_RUNNING,
    APP_IMU_DEGRADED,
    APP_IMU_FAULTED,
    APP_IMU_STOPPED,
} app_imu_state_t;

typedef struct {
    app_imu_state_t state;
    bool valid;                  /**< Successful, fresh sample and no current read error. */
    uint32_t sample_count;        /**< Saturating count of successful acquisitions. */
    uint32_t error_count;         /**< Saturating count of failed acquisitions/startup. */
    uint32_t consecutive_errors;
    int32_t last_error;           /**< ESP error code; zero after a successful read. */
    uint64_t timestamp_us;        /**< Device monotonic time after the last acquisition. */
    uint32_t age_ms;              /**< UINT32_MAX before any sample or clock anomaly. */
    float accel_g[3];            /**< Sensor-native X/Y/Z; includes gravity, range +/-4 g. */
    float gyro_dps[3];           /**< Sensor-native X/Y/Z, range +/-500 deg/s. */
} app_imu_snapshot_t;
