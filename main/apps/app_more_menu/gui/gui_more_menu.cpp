/**
 * @file gui_more_menu.cpp
 */
#include "gui_more_menu.h"
#include <cstdio>


void GUI_MoreMenu::init()
{
}


void GUI_MoreMenu::renderStatus(const std::string& line1, const std::string& line2)
{
    _canvas->fillScreen(_theme_color);
    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), 120, 115 - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), 120, 150);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_MoreMenu::renderPage(const std::string& name, float price, float absChg, float chg, int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    /* Stock name, directly on the themed background (no bubble),
       truncated with "..." if it doesn't fit */
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    std::string display_name = name;
    while (_canvas->textWidth(display_name.c_str()) > 200 && display_name.size() > 3)
    {
        display_name = display_name.substr(0, display_name.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_name.c_str(), 120, 62);

    /* Price */
    char price_buffer[24];
    snprintf(price_buffer, sizeof(price_buffer), "%.2f", price);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(price_buffer, 120, 100);

    /* Absolute change + percentage change, colored - red for up, green
       for down (A-share convention) */
    char chg_buffer[32];
    snprintf(chg_buffer, sizeof(chg_buffer), "%+.2f  %+.2f%%", absChg, chg);
    /* TFT_RED is LovyanGFX's RGB565 constant (0xF800), not a 24-bit RGB
       value like every other color in this codebase - passing it here
       would get reinterpreted as green, not red. Use an explicit 24-bit
       red instead. */
    uint32_t chg_color = (chg >= 0) ? 0xE53935U : 0x00B050U;
    _canvas->setTextColor(chg_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(chg_buffer, 120, 150);

    /* Position indicator, e.g. "3 / 11" */
    char position_buffer[16];
    snprintf(position_buffer, sizeof(position_buffer), "%d / %d", index + 1, count);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(position_buffer, 120, 190);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
