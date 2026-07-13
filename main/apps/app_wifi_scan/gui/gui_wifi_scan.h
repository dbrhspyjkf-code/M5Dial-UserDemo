/**
 * @file gui_wifi_scan.h
 * @brief Renderer for the DeepSeek balance viewer app. Pure display —
 * takes primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_WiFi_Scan : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the loading/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render the account balance
         *
         * @param totalBalance total balance, as a decimal string (e.g. "10.50")
         * @param currency currency code (e.g. "CNY")
         * @param grantedBalance free/granted balance
         * @param toppedUpBalance paid/topped-up balance
         */
        void renderPage(const std::string& totalBalance, const std::string& currency,
                         const std::string& grantedBalance, const std::string& toppedUpBalance);
};
