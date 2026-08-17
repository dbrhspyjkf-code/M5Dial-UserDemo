/**
 * @file app_reachy.h
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "../utilities/reachy_client/reachy_client.h"
#include "gui/gui_reachy.h"

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace REACHY
        {
            enum Page
            {
                CHAT = 0,
                VIDEO,
                AUDIO,
                MODE,
                SYSTEM,
                PAGE_COUNT,
            };

            enum AudioEdit
            {
                EDIT_NONE = 0,
                EDIT_VOLUME,
                EDIT_VAD,
            };

            enum ModeEdit
            {
                MODE_EDIT_NONE = 0,
                MODE_EDIT_BACKEND,
                MODE_EDIT_VOICE,
            };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                Page page = CHAT;
                AudioEdit audio_edit = EDIT_NONE;
                ModeEdit mode_edit = MODE_EDIT_NONE;
                REACHY_CLIENT::ChatTurn turn;
                REACHY_CLIENT::AudioState audio;
                REACHY_CLIENT::VideoState video;
                REACHY_CLIENT::JpegFrame camera_frame;
                REACHY_CLIENT::ModeState mode;
                std::string status = "Loading...";
                uint32_t last_fetch_ms = 0;
                uint32_t last_audio_fetch_ms = 0;
                uint32_t last_video_fetch_ms = 0;
                uint32_t last_frame_fetch_ms = 0;
                uint32_t last_audio_change_ms = 0;
                bool volume_dirty = false;
                bool vad_dirty = false;
                bool restart_confirm = false;
                bool touch_was_down = false;
            };
        }

        class AppReachy : public APP_BASE
        {
            private:
                const char* _tag = "reachy";
                REACHY::Data_t _data;
                GUI_Reachy _gui;

                void _fetch();
                void _fetch_audio();
                void _fetch_video();
                void _prepare_frame();
                void _fetch_frame();
                void _fetch_mode();
                void _render();
                void _handle_encoder();
                void _handle_touch();
                void _next_page(int direction);
                void _adjust_volume(int direction);
                void _ensure_mic_enabled();
                void _commit_audio_changes();
                void _restart_yrobot();
                void _toggle_mic();
                void _toggle_video();
                void _cycle_backend(int direction);
                void _cycle_voice(int direction);
                void _apply_mode();

            public:
                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
