/**
 * @file idle_screen.cpp
 */
#include "idle_screen.h"
#include "../../common_define.h"
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace IDLE_SCREEN
{
    static const uint32_t NIGHT_IDLE_MS = 60 * 1000;       /* 60s at night */
    static const int ON_BRIGHTNESS = 128;
    static const int NIGHT_START_H = 0;
    static const int NIGHT_END_H = 7;

    static bool s_initialized = false;
    static uint32_t s_last_activity_ms = 0;
    static int64_t s_last_encoder_count = 0;
    static bool s_screen_on = true;

    /* Wall-clock read for display: prefer the SYSTEM clock - after
       any successful NTP sync it is maintained by ESP32 internally and
       never freezes. The BM8563 RTC was observed stopping (screen
       frozen at a fixed time while everything else kept running);
       RTC is only the boot-time seed when NTP has not synced yet. */
    static void _local_time(struct tm& out)
    {
        time_t now;
        time(&now);
        localtime_r(&now, &out);
    }

    static bool _is_night(HAL::HAL* hal)
    {
        struct tm time_now;
        _local_time(time_now);
        return (time_now.tm_hour >= NIGHT_START_H && time_now.tm_hour < NIGHT_END_H);
    }

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
            delay(20);
            touched = hal->tp.isTouched();
        }

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

                /* Absorb the wake gesture */
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
                    hal->encoder.wasMoved(true);
                }

                s_last_activity_ms = millis();
                return true;
            }

            return false;
        }

        /* Only sleep during night hours (00:00-06:59) */
        if (s_screen_on && _is_night(hal) && (millis() - s_last_activity_ms > NIGHT_IDLE_MS))
        {
            hal->display.setBrightness(0);
            s_screen_on = false;
        }

        /* Stay dark: anything that re-initializes the display (e.g. the
           TP driver's L3 hardware-reset heal re-running display.init())
           restores default brightness, so re-assert screen-off every tick.
           Idempotent and cheap. */
        if (!s_screen_on && _is_night(hal))
            hal->display.setBrightness(0);

        return false;
    }
}
