/**
 * @file gui_reachy.h
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include "../../utilities/reachy_client/reachy_client.h"
#include <string>

class GUI_Reachy : public GUI_Base
{
    public:
        void init() override;
        void renderPage(const std::string& status,
                        const std::string& user,
                        const std::string& assistant);
        void renderChat(const std::string& user,
                        const std::string& assistant,
                        int volume_percent = -1);
        void renderAudio(const REACHY_CLIENT::AudioState& audio,
                         int edit_mode);
        void renderSystem(const std::string& status);
        void renderVideo(const REACHY_CLIENT::VideoState& video,
                         const REACHY_CLIENT::JpegFrame& frame,
                         const std::string& status);
        void renderMode(const REACHY_CLIENT::ModeState& mode,
                        int edit_mode,
                        bool restart_confirm,
                        const std::string& status);
};
