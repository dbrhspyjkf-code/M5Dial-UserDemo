/**
 * @file gui_temp_demo.h
 * @brief Renderer for the AC control app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_VideoShit : public GUI_Base
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
         * @param acOn current power state (affects the POWER button label)
         * @param targetTemp target temperature in °C
         * @param modeName current HVAC mode name (e.g. "cool", "off")
         */
        void renderPage(bool acOn, float targetTemp, const std::string& modeName);
};
