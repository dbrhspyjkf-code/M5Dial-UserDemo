/**
 * @file idle_screen.h
 * @brief Auto screen-off during night hours (00:00-07:00) after 60s of
 * inactivity. Any touch/encoder/button wakes it back up. Outside night
 * hours the screen stays on.
 */
#pragma once
#include "../../../hal/hal.h"

namespace IDLE_SCREEN
{
    /**
     * @brief Call once per loop iteration, before calling onRunning().
     *
     * @return true only on the cycle where this call just woke the
     * screen — the caller should skip onRunning() for that one cycle.
     * Returns false on every other cycle.
     */
    bool tick(HAL::HAL* hal);
}
