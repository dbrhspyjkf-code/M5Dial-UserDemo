/**
 * @file app_temp_demo.h
 * @brief Controls the master bedroom air conditioner's power, HVAC
 * mode, and target temperature through Home Assistant's REST API.
 * Touch: power/mode toggles. Encoder: debounced target temperature.
 * Encoder button: quit (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_temp_demo.h"
#include "ac_config.h"
#include <vector>


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace VIDEO_SHIT
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                bool ac_on = false;
                std::string hvac_mode;          // current mode, "off" when powered off
                std::string last_active_mode;    // remembered mode to restore on power-on
                std::vector<std::string> hvac_modes;
                float target_temp = 26.0f;
                float min_temp = 16.0f;
                float max_temp = 30.0f;

                bool temp_dirty = false;
                uint32_t last_temp_change_ms = 0;
            };
        } // namespace VIDEO_SHIT

        class VideoShit : public APP_BASE
        {
            private:
                const char* _tag = "ac";

                void _handle_encoder();
                void _handle_touch();
                void _handle_temp_debounce();
                void _render();
                void _refresh_state();

            public:
                VideoShit() = default;
                ~VideoShit() = default;

                VIDEO_SHIT::Data_t _data;
                GUI_VideoShit _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
