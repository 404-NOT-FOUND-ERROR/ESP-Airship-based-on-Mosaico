// SPDX-License-Identifier: Apache-2.0
#include "app_display.h"
#include "app_airboat_ui.h"
#include "app_imu.h"
#include "app_motor_control.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "iris_ota_support.h"
#include "iris_screen_mirror.h"
#include "nvs_flash.h"
#include "ui_bundle.h"

static const char *TAG = "airboat_baseline";

/* The composition root retains owned handles, never controller internals. */
static struct {
    esp_gsp_handle_t ui;
    app_motor_control_handle_t motor_control;
    app_airboat_ui_handle_t controller;
    app_imu_handle_t imu;
} application;

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());
    /* Recovery must remain reachable even when the UI bundle cannot be loaded. */
    iris_ota_support_start();

    esp_gsp_config_t config;
    const esp_err_t err = ui_bundle_open(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GSP UI unavailable (0x%x); Recovery RPC remains active", err);
        return;
    }
    esp_gsp_esp_lcd_config_t display;
    const esp_err_t display_err = app_display_init(&display);
    if (display_err != ESP_OK) {
        ESP_LOGE(TAG, "Display unavailable (0x%x); Recovery RPC remains active", display_err);
        return;
    }
    ESP_ERROR_CHECK(iris_screen_mirror_init());
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&config, &display, &application.ui));
    ESP_ERROR_CHECK(iris_screen_mirror_attach(application.ui));
    const app_motor_control_result_t motor_err =
        app_motor_control_create(&application.motor_control);
    if (motor_err != APP_MOTOR_CONTROL_OK) {
        ESP_LOGE(TAG, "Motor control service unavailable (%d)", motor_err);
        return;
    }
    const esp_gsp_err_t ui_err = app_airboat_ui_create(
        application.ui, application.motor_control, &application.controller);
    if (ui_err != ESP_GSP_OK) {
        if (!application.controller) {
            (void)app_motor_control_destroy(&application.motor_control);
        }
        ESP_ERROR_CHECK(ui_err);
    }
    const esp_err_t imu_err = app_imu_start(&application.imu);
    if (imu_err != ESP_OK) {
        ESP_LOGE(TAG, "IMU task unavailable: %s", esp_err_to_name(imu_err));
    }
    /* GSP retains the callback context. Device UI/controller live until reboot;
     * teardown requires quiescing GSP before app_airboat_ui_destroy(). */
    ESP_LOGI(TAG, "AirBoat software baseline ready");
}
