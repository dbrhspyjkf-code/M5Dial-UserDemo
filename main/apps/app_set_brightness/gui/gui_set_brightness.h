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
         * @brief Render the normal brightness-control screen
         *
         * @param brightnessPct 0-100
         * @param lightOn current on/off state (affects the toggle button label)
         */
        void renderPage(int brightnessPct, bool lightOn);
};
