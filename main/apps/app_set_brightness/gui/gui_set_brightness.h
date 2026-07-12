/**
 * @file gui_set_brightness.h
 * @brief Renderer for the fish tank light control app. Pure display —
 * takes primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_SetBrightness : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the connecting/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render the normal control screen
         *
         * @param brightnessMode true = show/adjust brightness, false = show/adjust effect
         * @param brightnessPct 0-100 (shown when brightnessMode is true)
         * @param effectName current effect name, or "(no effects)" (shown when brightnessMode is false)
         * @param lightOn current on/off state (affects the ON/OFF button label)
         */
        void renderPage(bool brightnessMode, int brightnessPct,
                         const std::string& effectName, bool lightOn);
};
