/**
 * @file ntp_sync.h
 * @brief One-shot SNTP time sync at boot, writing the result into the
 * device's battery-backed PCF8563 RTC so it's accurate from a real
 * time source instead of just whatever it last had.
 */
#pragma once
#include "../../../hal/hal.h"
#include <cstdint>

namespace NTP_SYNC
{
    /**
     * @brief Start SNTP, wait up to timeout_ms for it to complete, and
     * write the resulting time into the RTC. If it times out (no
     * network, bad NTP response), the RTC is left unchanged - this is
     * a best-effort accuracy improvement, not a hard dependency.
     */
    void sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms);
}
