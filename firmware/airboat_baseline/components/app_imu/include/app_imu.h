// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "app_imu_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app_imu *app_imu_handle_t;

/** Start the single boot-lifetime sampler; hardware readiness is asynchronous. */
esp_err_t app_imu_start(app_imu_handle_t *out_handle);

/** Copy a fresh validity assessment; callable from task context across owners. */
esp_err_t app_imu_read(app_imu_handle_t handle, app_imu_snapshot_t *out_snapshot);

/** Request terminal stop and wait at most timeout_ms; retain handle on timeout. */
esp_err_t app_imu_stop(app_imu_handle_t handle, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
