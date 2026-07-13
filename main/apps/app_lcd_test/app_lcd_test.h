/**
 * @file app_lcd_test.h
 * @brief Controls a TV's power, volume, and navigation (D-pad) through
 * Home Assistant's REST API. Power is inverted through a "sound bar
 * mode" switch (ON = TV off) - this file is the only place that
 * inversion is visible; the GUI and touch handling only ever deal with
 * the TV's actual on/off state. Two pages, toggled by touch: VOLUME
 * (power/volume) and NAV (up/down/left/right/ok/back/menu, all
 * momentary button-domain presses with nothing to read back). Encoder:
 * debounced volume, VOLUME page only. Encoder button: quit
 * (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_lcd_test.h"
#include "tv_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace LCD_TEST
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };
            enum class Page { VOLUME, NAV };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;
                Page page = Page::VOLUME;

                bool tv_on = true;
                float volume = 50.0f;
                float volume_min = 0.0f;
                float volume_max = 100.0f;

                bool volume_dirty = false;
                uint32_t last_volume_change_ms = 0;
            };
        }

        class LCD_Test : public APP_BASE
        {
            private:
                const char* _tag = "lcd";

                void _handle_encoder();
                void _handle_touch();
                void _handle_touch_volume_page(int x, int y);
                void _handle_touch_nav_page(int x, int y);
                void _handle_volume_debounce();
                void _render();
                void _refresh_state();

            public:
                LCD_TEST::Data_t _data;
                GUI_LCD_TEST _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
