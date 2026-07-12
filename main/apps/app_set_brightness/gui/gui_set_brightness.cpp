/**
 * @file gui_set_brightness.cpp
 */
#include "gui_set_brightness.h"
#include <cstdio>


void GUI_SetBrightness::init()
{
}


void GUI_SetBrightness::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_SetBrightness::renderPage(int brightnessPct, bool lightOn)
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

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", brightnessPct);

    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 56);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("FISH LIGHT", bubble.x, bubble.y + 26);

    /* On/off toggle button, below the bubble (bubble bottom edge is at
       bubble.y + bubble.height/2 = 190) */
    int btn_x = 120;
    int btn_y = 202;
    int btn_height = 24;
    const char* label = lightOn ? "ON" : "OFF";

    _canvas->setTextSize(1);
    int text_w = _canvas->textWidth(label);
    int text_h = _canvas->fontHeight();
    int btn_width = text_w + 30;
    if (btn_width < 60) btn_width = 60;

    _canvas->fillSmoothRoundRect(btn_x - btn_width / 2, btn_y - btn_height / 2,
                                  btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(label, btn_x, btn_y - text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
