/**
 * @file gui_lcd_test.h
 * @brief Renderer for the TV control app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_LCD_TEST : public GUI_Base
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
         * @param volumePct 0-100, already normalized from the entity's actual min/max range
         * @param tvOn true if the TV is on (already inverted from the underlying switch)
         */
        void renderPage(int volumePct, bool tvOn);
};
