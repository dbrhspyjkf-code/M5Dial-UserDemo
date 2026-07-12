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

    BasicObeject_t bubble;
    bubble.x = 120;
    bubble.y = 120;
    bubble.width = 240;
    bubble.height = 140;
    _canvas->fillSmoothRoundRect(bubble.x - bubble.width / 2, bubble.y - bubble.height / 2,
                                  bubble.width, bubble.height, 36, TFT_WHITE);

    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), bubble.x, bubble.y - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), bubble.x, bubble.y + 30);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_MoreMenu::renderPage(const std::string& name, float price, float chg, int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    BasicObeject_t bubble;
    bubble.x = 120;
    bubble.y = 120;
    bubble.width = 240;
    bubble.height = 140;
    _canvas->fillSmoothRoundRect(bubble.x - bubble.width / 2, bubble.y - bubble.height / 2,
                                  bubble.width, bubble.height, 36, TFT_WHITE);

    /* Stock name, truncated with "..." if it doesn't fit */
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    std::string display_name = name;
    while (_canvas->textWidth(display_name.c_str()) > 200 && display_name.size() > 3)
    {
        display_name = display_name.substr(0, display_name.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_name.c_str(), bubble.x, bubble.y - 48);

    /* Price, large */
    char price_buffer[24];
    snprintf(price_buffer, sizeof(price_buffer), "%.2f", price);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(price_buffer, bubble.x, bubble.y - 26);

    /* Change %, colored - red for up, green for down (A-share convention) */
    char chg_buffer[24];
    snprintf(chg_buffer, sizeof(chg_buffer), "%+.2f%%", chg);
    uint32_t chg_color = (chg >= 0) ? TFT_RED : 0x00B050U;
    _canvas->setTextColor(chg_color);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(chg_buffer, bubble.x, bubble.y + 20);

    /* Position indicator, e.g. "3 / 11" */
    char position_buffer[16];
    snprintf(position_buffer, sizeof(position_buffer), "%d / %d", index + 1, count);
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(position_buffer, bubble.x, bubble.y + 50);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
