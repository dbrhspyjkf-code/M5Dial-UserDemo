/**
 * @file gui_more_menu.h
 * @brief Renderer for the stock watchlist app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_MoreMenu : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the loading/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render one stock
         *
         * @param name stock name (or code, if name is empty)
         * @param price current price
         * @param chg percentage change (positive = up, negative = down)
         * @param index 0-based position in the list
         * @param count total number of stocks
         */
        void renderPage(const std::string& name, float price, float chg, int index, int count);
};
