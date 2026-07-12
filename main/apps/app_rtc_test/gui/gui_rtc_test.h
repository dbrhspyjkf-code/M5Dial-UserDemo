/**
 * @file gui_rtc_test.h
 * @brief Renderer for the fan control app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_RTC_Test : public GUI_Base
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
         * @param fanOn current on/off state (affects the POWER button label)
         * @param percentage 0-100 fan speed
         * @param oscillating current swing state (affects the SWING button highlight)
         */
        void renderPage(bool fanOn, int percentage, bool oscillating);
};
