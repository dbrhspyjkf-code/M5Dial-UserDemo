/**
 * @file gui_wifi_scan.cpp
 */
#include "gui_wifi_scan.h"
#include <cstdio>


void GUI_WiFi_Scan::init()
{
}


void GUI_WiFi_Scan::renderStatus(const std::string& line1, const std::string& line2)
{
    _canvas->fillScreen(_theme_color);
    _draw_top_icon();

    _canvas->setFont(&fonts::Font0);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), 120, 115 - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), 120, 150);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_WiFi_Scan::renderPage(const std::string& totalBalance, const std::string& currency,
                                const std::string& grantedBalance, const std::string& toppedUpBalance)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setFont(&fonts::Font0);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    _canvas->drawCenterString("DEEPSEEK", 120, 52);

    char total_buf[32];
    snprintf(total_buf, sizeof(total_buf), "%s %s", totalBalance.c_str(), currency.c_str());
    _canvas->setTextSize(3);
    int total_h = _canvas->fontHeight();
    _canvas->drawCenterString(total_buf, 120, 120 - total_h / 2);

    char topup_buf[24];
    snprintf(topup_buf, sizeof(topup_buf), "topup %s", toppedUpBalance.c_str());
    char free_buf[24];
    snprintf(free_buf, sizeof(free_buf), "free %s", grantedBalance.c_str());

    _canvas->setTextSize(2);
    _canvas->drawCenterString(topup_buf, 120, 160);
    _canvas->drawCenterString(free_buf, 120, 182);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
