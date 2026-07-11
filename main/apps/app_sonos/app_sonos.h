/**
 * @file app_sonos.h
 * @brief Controls a Sonos speaker through Home Assistant's REST API.
 * Touch: prev/play-pause/next. Encoder: volume (debounced). Encoder
 * button: quit (project-wide convention, same as every other app).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_sonos.h"
#include "sonos_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace SONOS
        {
            enum class State { CONNECTING, POLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                std::string title;
                std::string artist;
                bool is_playing = false;
                float volume = 0.5f;

                uint32_t last_poll_ms = 0;
                int poll_fail_count = 0;

                /* Encoder volume debounce */
                bool volume_dirty = false;
                uint32_t last_volume_change_ms = 0;
            };
        }

        class AppSonos : public APP_BASE
        {
            private:
                const char* _tag = "AppSonos";

                void _handle_encoder();
                void _handle_touch();
                void _handle_poll();
                void _handle_volume_debounce();
                void _render();
                void _refresh_state();

            public:
                SONOS::Data_t _data;
                GUI_Sonos _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
