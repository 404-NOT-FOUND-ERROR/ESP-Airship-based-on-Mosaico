// SPDX-License-Identifier: Apache-2.0
#include "app_imu.h"

#include <inttypes.h>
#include <stdlib.h>

#include "app_imu_model.h"
#include "bsp/esp_mosaico.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define IMU_TASK_STACK_BYTES 4096U
#define IMU_TASK_PRIORITY 3U
#define IMU_WARMUP_MS 100U
#define IMU_FAULT_RETRY_MS 200U
#define IMU_LOG_INTERVAL_US 1000000ULL
#define IMU_STOP_MAX_MS 60000U
#define IMU_DIAGNOSTIC_WARMUP_US 5000000ULL

static const char *TAG = "app_imu";
/* BSP owns one global BMI270. Reservation is held for the boot lifetime. */
static portMUX_TYPE reservation_lock = portMUX_INITIALIZER_UNLOCKED;
static bool reserved;

struct app_imu {
    portMUX_TYPE lock;
    app_imu_snapshot_t snapshot;
    SemaphoreHandle_t finished_signal;
    bool stop_requested;
    bool finished;
    esp_err_t cleanup_error;
};

static bool should_stop(struct app_imu *state)
{
    portENTER_CRITICAL(&state->lock);
    const bool stop = state->stop_requested;
    portEXIT_CRITICAL(&state->lock);
    return stop;
}

static void publish_error(struct app_imu *state, esp_err_t error, bool terminal)
{
    portENTER_CRITICAL(&state->lock);
    app_imu_model_error(&state->snapshot, error);
    if (terminal) {
        state->snapshot.state = APP_IMU_FAULTED;
    }
    portEXIT_CRITICAL(&state->lock);
}

static esp_err_t read_hardware(struct bmi2_dev *device, int16_t accel[3],
                                int16_t gyro[3])
{
    uint8_t status = 0;
    if (bmi2_get_status(&status, device) != BMI2_OK) {
        return ESP_FAIL;
    }
    const uint8_t required = BMI2_DRDY_ACC | BMI2_DRDY_GYR;
    if ((status & required) != required) {
        return ESP_ERR_NOT_FINISHED;
    }
    struct bmi2_sens_data data = {0};
    if (bmi2_get_sensor_data(&data, device) != BMI2_OK) {
        return ESP_FAIL;
    }
    accel[0] = data.acc.x;
    accel[1] = data.acc.y;
    accel[2] = data.acc.z;
    gyro[0] = data.gyr.x;
    gyro[1] = data.gyr.y;
    gyro[2] = data.gyr.z;
    return ESP_OK;
}

static void log_snapshot(struct app_imu *state)
{
    app_imu_snapshot_t snapshot;
    if (app_imu_read(state, &snapshot) != ESP_OK) {
        return;
    }
    ESP_LOGI(TAG, "IMU n=%" PRIu32 " t_us=%" PRIu64 " state=%u valid=%u "
             "a_g=%.4f,%.4f,%.4f w_dps=%.3f,%.3f,%.3f errors=%" PRIu32 " err=%" PRId32,
             snapshot.sample_count, snapshot.timestamp_us, (unsigned)snapshot.state,
             (unsigned)snapshot.valid,
             (double)snapshot.accel_g[0], (double)snapshot.accel_g[1],
             (double)snapshot.accel_g[2], (double)snapshot.gyro_dps[0],
             (double)snapshot.gyro_dps[1], (double)snapshot.gyro_dps[2],
             snapshot.error_count, snapshot.last_error);
}

static void sampler_task(void *argument)
{
    struct app_imu *state = argument;
    const bsp_imu_config_t config = {
        .accel_odr = BMI2_ACC_ODR_100HZ,
        .accel_range = BMI2_ACC_RANGE_4G,
        .gyro_odr = BMI2_GYR_ODR_100HZ,
        .gyro_range = BMI2_GYR_RANGE_500,
    };
    esp_err_t error = bsp_imu_init();
    const bool initialized = error == ESP_OK;
    if (error == ESP_OK) {
        error = bsp_imu_start(&config);
    }
    struct bmi2_dev *device = error == ESP_OK ? bsp_imu_get_handle() : NULL;
    if (error == ESP_OK && !device) {
        error = ESP_ERR_INVALID_STATE;
    }
    const bool startup_failed = error != ESP_OK;
    if (startup_failed) {
        publish_error(state, error, true);
        ESP_LOGE(TAG, "BMI270 startup failed: %s", esp_err_to_name(error));
    } else {
        /* 100 Hz ODR needs a settled first conversion before accepting samples. */
        vTaskDelay(pdMS_TO_TICKS(IMU_WARMUP_MS));
        ESP_LOGI(TAG, "BMI270 sensor frame; accel +/-4g, gyro +/-500dps, polling 50Hz");
    }

    uint64_t next_log_us = 0;
#if CONFIG_AIRBOAT_IMU_DIAGNOSTIC_FAULT_TEST
    uint64_t diagnostic_first_success_us = 0;
    uint32_t diagnostic_failures_remaining = 0;
    bool diagnostic_fault_started = false;
    bool diagnostic_fault_reported = false;
    bool diagnostic_recovery_reported = false;
#endif
    while (!startup_failed && !should_stop(state)) {
        int16_t accel[3];
        int16_t gyro[3];
        error = read_hardware(device, accel, gyro);
        const uint64_t now_us = (uint64_t)esp_timer_get_time();
#if CONFIG_AIRBOAT_IMU_DIAGNOSTIC_FAULT_TEST
        if (error == ESP_OK && diagnostic_first_success_us == 0U) {
            diagnostic_first_success_us = now_us;
        }
        if (error == ESP_OK && !diagnostic_fault_started &&
            now_us - diagnostic_first_success_us >= IMU_DIAGNOSTIC_WARMUP_US) {
            diagnostic_fault_started = true;
            diagnostic_failures_remaining = APP_IMU_FAULT_THRESHOLD;
            ESP_LOGW(TAG, "diagnostic: injecting %" PRIu32 " application read failures",
                     diagnostic_failures_remaining);
        }
        if (error == ESP_OK && diagnostic_failures_remaining > 0U) {
            const uint32_t injected = APP_IMU_FAULT_THRESHOLD -
                                      diagnostic_failures_remaining + 1U;
            diagnostic_failures_remaining--;
            error = ESP_FAIL;
            ESP_LOGW(TAG, "diagnostic: injected read failure %" PRIu32 "/%u",
                     injected, APP_IMU_FAULT_THRESHOLD);
        }
#endif
        uint32_t failures;
#if CONFIG_AIRBOAT_IMU_DIAGNOSTIC_FAULT_TEST
        app_imu_state_t observed_state;
        uint32_t observed_error_count;
        uint32_t observed_consecutive_errors;
#endif
        portENTER_CRITICAL(&state->lock);
        if (error == ESP_OK &&
            !app_imu_model_sample(&state->snapshot, accel, gyro, now_us)) {
            error = ESP_ERR_INVALID_RESPONSE;
        }
        if (error != ESP_OK) {
            app_imu_model_error(&state->snapshot, error);
        }
        failures = state->snapshot.consecutive_errors;
#if CONFIG_AIRBOAT_IMU_DIAGNOSTIC_FAULT_TEST
        observed_state = state->snapshot.state;
        observed_error_count = state->snapshot.error_count;
        observed_consecutive_errors = state->snapshot.consecutive_errors;
#endif
        portEXIT_CRITICAL(&state->lock);
#if CONFIG_AIRBOAT_IMU_DIAGNOSTIC_FAULT_TEST
        if (diagnostic_fault_started && diagnostic_failures_remaining == 0U &&
            !diagnostic_fault_reported && observed_state == APP_IMU_FAULTED) {
            diagnostic_fault_reported = true;
            ESP_LOGW(TAG, "diagnostic: FAULTED confirmed errors=%" PRIu32
                     " t_us=%" PRIu64 " retry_ms=%u",
                     observed_error_count, now_us, IMU_FAULT_RETRY_MS);
            log_snapshot(state);
        }
        if (diagnostic_fault_reported && !diagnostic_recovery_reported &&
            error == ESP_OK && observed_state == APP_IMU_RUNNING &&
            observed_consecutive_errors == 0U) {
            diagnostic_recovery_reported = true;
            ESP_LOGI(TAG, "diagnostic: RUNNING restored after %" PRIu32
                     " errors t_us=%" PRIu64 " consecutive=%" PRIu32,
                     observed_error_count, now_us, observed_consecutive_errors);
            log_snapshot(state);
        }
#endif
        if (now_us >= next_log_us) {
            log_snapshot(state);
            next_log_us = now_us + IMU_LOG_INTERVAL_US;
        }
        vTaskDelay(pdMS_TO_TICKS(failures >= APP_IMU_FAULT_THRESHOLD ?
                               IMU_FAULT_RETRY_MS : APP_IMU_PERIOD_MS));
    }

    /* BSP may retain a partial handle on failure. It owns shared bus/rail policy. */
    esp_err_t cleanup = initialized ? bsp_imu_stop() : ESP_OK;
    if (cleanup == ESP_OK) {
        cleanup = bsp_imu_deinit();
    }
    portENTER_CRITICAL(&state->lock);
    state->cleanup_error = cleanup;
    state->snapshot.valid = false;
    if (cleanup != ESP_OK) {
        app_imu_model_error(&state->snapshot, cleanup);
        state->snapshot.state = APP_IMU_FAULTED;
    } else if (!startup_failed) {
        state->snapshot.state = APP_IMU_STOPPED;
    }
    state->finished = true;
    portEXIT_CRITICAL(&state->lock);
    if (cleanup != ESP_OK) {
        ESP_LOGE(TAG, "BMI270 cleanup failed: %s; shared resources retained", esp_err_to_name(cleanup));
    }
    /* Context/semaphore remain valid until reboot; no cross-core delete/free race. */
    xSemaphoreGive(state->finished_signal);
    vTaskDelete(NULL);
}

esp_err_t app_imu_start(app_imu_handle_t *out_handle)
{
    if (!out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;
    portENTER_CRITICAL(&reservation_lock);
    const bool already_reserved = reserved;
    reserved = true;
    portEXIT_CRITICAL(&reservation_lock);
    if (already_reserved) {
        return ESP_ERR_INVALID_STATE;
    }
    struct app_imu *state = calloc(1, sizeof(*state));
    if (state) {
        state->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
        app_imu_model_init(&state->snapshot);
        state->finished_signal = xSemaphoreCreateBinary();
        if (state->finished_signal &&
            xTaskCreate(sampler_task, "airboat_imu", IMU_TASK_STACK_BYTES, state,
                        IMU_TASK_PRIORITY, NULL) == pdPASS) {
            *out_handle = state;
            return ESP_OK;
        }
        if (state->finished_signal) {
            vSemaphoreDelete(state->finished_signal);
        }
        free(state);
    }
    portENTER_CRITICAL(&reservation_lock);
    reserved = false;
    portEXIT_CRITICAL(&reservation_lock);
    return ESP_ERR_NO_MEM;
}

esp_err_t app_imu_read(app_imu_handle_t handle, app_imu_snapshot_t *out_snapshot)
{
    if (!handle || !out_snapshot) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&handle->lock);
    /* Timestamp inside the same lock prevents an age/read ordering race. */
    app_imu_model_read(&handle->snapshot, (uint64_t)esp_timer_get_time(), out_snapshot);
    portEXIT_CRITICAL(&handle->lock);
    return ESP_OK;
}

esp_err_t app_imu_stop(app_imu_handle_t handle, uint32_t timeout_ms)
{
    if (!handle || timeout_ms > IMU_STOP_MAX_MS) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&handle->lock);
    handle->stop_requested = true;
    const bool finished = handle->finished;
    portEXIT_CRITICAL(&handle->lock);
    if (!finished && xSemaphoreTake(handle->finished_signal,
                                    pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    portENTER_CRITICAL(&handle->lock);
    const esp_err_t error = handle->cleanup_error;
    portEXIT_CRITICAL(&handle->lock);
    return error;
}
