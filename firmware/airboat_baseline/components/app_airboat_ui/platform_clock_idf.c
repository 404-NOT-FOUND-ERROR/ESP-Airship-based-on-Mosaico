// SPDX-License-Identifier: Apache-2.0

#include "app_airboat_clock.h"

#include "esp_timer.h"

uint32_t app_airboat_clock_now_ms(void)
{
    const int64_t now_us = esp_timer_get_time();
    return (uint32_t)(now_us / INT64_C(1000));
}
