/**
 * @file app_set_brightness.h
 * @brief Controls the fish tank light's brightness and on/off state
 * through Home Assistant's REST API. Touch: on/off toggle. Encoder:
 * debounced brightness. Encoder button: quit (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_set_brightness.h"
#include "fishtank_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace SET_BRIGHTNESS
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                int brightness_pct = 50;
                bool light_on = true;

                bool brightness_dirty = false;
                uint32_t last_brightness_change_ms = 0;
            };
        }

        class Set_Brightness : public APP_BASE
        {
            private:
                const char* _tag = "brightness";

                void _handle_encoder();
                void _handle_touch();
                void _handle_brightness_debounce();
                void _render();
                void _refresh_state();

            public:
                SET_BRIGHTNESS::Data_t _data;
                GUI_SetBrightness _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
