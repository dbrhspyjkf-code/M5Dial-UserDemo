/**
 * @file idle_screen.h
 * @brief After 5 minutes with no touch/encoder/button activity, turns
 * the screen backlight off to save power. The next touch, encoder
 * rotation, or encoder-button press wakes it back up. Hooked into the
 * project's 2 shared while(1) loop sites (main.cpp's launcher loop and
 * Launcher::_simple_app_manager, which drives every app) instead of
 * any individual app, so no app file needs to change.
 */
#pragma once
#include "../../../hal/hal.h"

namespace IDLE_SCREEN
{
    /**
     * @brief Call once per loop iteration, before calling onRunning().
     *
     * @return true only on the exact cycle where this call just woke
     * the screen from sleep and consumed the gesture that woke it
     * (touch release / button release / pending encoder rotation) -
     * the caller should skip onRunning() for that one cycle so the
     * wake gesture can't also trigger an action. Returns false on
     * every other cycle (screen already on, or already asleep with no
     * new activity yet) - the caller should call onRunning() as usual.
     */
    bool tick(HAL::HAL* hal);
}
