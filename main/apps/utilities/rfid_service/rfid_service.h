/**
 * @file rfid_service.h
 * @brief Boot-time singleton that owns the RC522 reader for the whole
 * device's lifetime (not tied to any app's onCreate/onDestroy). Tapping
 * the configured phone card triggers a Home Assistant turn-off action
 * regardless of which app, if any, is currently open.
 */
#pragma once
#include <cstdint>
#include "../../../hal/hal.h"

namespace RFID_SERVICE
{
    /**
     * @brief Create and start the RC522 reader, register the scan event
     * handler. Call exactly once, at boot, before the launcher starts.
     */
    void init(HAL::HAL* hal);

    /**
     * @brief The most recently scanned card's serial number this boot,
     * or 0 if nothing has been scanned yet.
     */
    uint64_t last_scanned_sn();
}
