/**
 * @file gui_sonos.h
 * @brief Renderer for the Sonos app. Pure display — takes primitive
 * values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_Sonos : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the connecting/error screen (no track info yet)
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render the normal playing/paused screen
         *
         * @param title track title (may be empty)
         * @param artist track artist (may be empty)
         * @param isPlaying true if state == "playing" (affects play/pause glyph)
         * @param volumePercent 0-100
         */
        void renderPlaying(const std::string& title, const std::string& artist,
                            bool isPlaying, int volumePercent);
};
