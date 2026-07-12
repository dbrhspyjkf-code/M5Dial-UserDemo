/**
 * @file gui_rtc_test.cpp
 */
#include "gui_rtc_test.h"
#include <cstdio>


void GUI_RTC_Test::init()
{
}


void GUI_RTC_Test::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_RTC_Test::renderPage(bool fanOn, int percentage, bool oscillating)
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
    _canvas->setTextSize(1);
    _canvas->drawCenterString("SPEED", bubble.x, bubble.y - 48);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", percentage);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 30);

    _canvas->setTextSize(1);
    _canvas->drawCenterString("FAN", bubble.x, bubble.y + 34);

    /* POWER and SWING buttons, side by side below the bubble */
    int btn_y = 206;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* power_label = fanOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 85;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    /* SWING button: filled with the theme accent when oscillating is on,
       white otherwise - same on/off highlight convention used for the
       selected field in the Timer app */
    const char* swing_label = "SWING";
    int swing_text_w = _canvas->textWidth(swing_label);
    int swing_text_h = _canvas->fontHeight();
    int swing_btn_width = swing_text_w + 30;
    if (swing_btn_width < 60) swing_btn_width = 60;
    int swing_btn_x = 155;

    uint32_t swing_btn_color = oscillating ? TFT_WHITE : 0x666666U;
    uint32_t swing_text_color = oscillating ? _theme_color : TFT_WHITE;

    _canvas->fillSmoothRoundRect(swing_btn_x - swing_btn_width / 2, btn_y - btn_height / 2,
                                  swing_btn_width, btn_height, btn_height / 2, swing_btn_color);
    _canvas->setTextColor(swing_text_color);
    _canvas->drawCenterString(swing_label, swing_btn_x, btn_y - swing_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
