// SPDX-License-Identifier: Apache-2.0

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "app_airboat_clock.h"

uint32_t app_airboat_clock_now_ms(void)
{
    return (uint32_t)GetTickCount64();
}
