/**
 * @file idle_screen.cpp
 */
#include "idle_screen.h"
#include "../../common_define.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace IDLE_SCREEN
{
    static const uint32_t IDLE_TIMEOUT_MS = 5 * 60 * 1000;
    static const int ON_BRIGHTNESS = 128;

    static bool s_initialized = false;
    static uint32_t s_last_activity_ms = 0;
    static int64_t s_last_encoder_count = 0;
    static bool s_screen_on = true;

    bool tick(HAL::HAL* hal)
    {
        if (!s_initialized)
        {
            s_last_activity_ms = millis();
            s_last_encoder_count = hal->encoder.getCount();
            s_initialized = true;
        }

        bool touched = hal->tp.isTouched();
        if (touched)
        {
            /* TP_FT3267::isTouched() is a bare I2C register read with no
               error checking - an occasional bus glitch can report a
               phantom touch for a single poll, and tick() is polled at
               very high frequency while idle. A real finger touch holds
               for far longer than one poll, so require it to still read
               true after a short delay before trusting it (same fix
               applied to the launcher's screensaver after a phantom
               touch was observed live waking it after only ~31s idle). */
            delay(20);
            touched = hal->tp.isTouched();
        }

        /* Read the raw count directly (no side effects) instead of
           wasMoved(), which mutates its own internal _last_count and
           would otherwise silently consume rotation before the open
           app's own onRunning() gets a chance to see it. */
        int64_t current_count = hal->encoder.getCount();
        bool encoder_moved = (current_count != s_last_encoder_count);
        s_last_encoder_count = current_count;

        bool button_pressed = !hal->encoder.btn.read();

        bool activity = touched || encoder_moved || button_pressed;

        if (activity)
        {
            s_last_activity_ms = millis();

            if (!s_screen_on)
            {
                hal->display.setBrightness(ON_BRIGHTNESS);
                s_screen_on = true;

                /* Absorb the whole gesture that woke the screen, same
                   "wake, don't act" behavior as a phone lock screen. */
                if (touched)
                {
                    while (hal->tp.isTouched())
                    {
                        hal->tp.update();
                        delay(5);
                    }
                }
                if (button_pressed)
                {
                    while (!hal->encoder.btn.read())
                        delay(5);
                }
                if (encoder_moved)
                {
                    /* Absorb the pending rotation into wasMoved()'s own
                       tracking so the open app's next wasMoved() call
                       doesn't see leftover movement from before wake. */
                    hal->encoder.wasMoved(true);
                }

                s_last_activity_ms = millis();
                return true;
            }

            return false;
        }

        if (s_screen_on && (millis() - s_last_activity_ms > IDLE_TIMEOUT_MS))
        {
            hal->display.setBrightness(0);
            s_screen_on = false;
        }

        return false;
    }
}
