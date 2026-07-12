/**
 * @file gui_temp_demo.cpp
 */
#include "gui_temp_demo.h"
#include <cstdio>


void GUI_VideoShit::init()
{
}


void GUI_VideoShit::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_VideoShit::renderPage(bool acOn, float targetTemp, const std::string& modeName)
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
    _canvas->drawCenterString("TARGET", bubble.x, bubble.y - 48);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%.1f%cC", targetTemp, 176 /* degree symbol, ASCII 0xB0 */);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 30);

    _canvas->setTextSize(1);
    std::string mode_display = acOn ? modeName : "OFF";
    _canvas->drawCenterString(mode_display.c_str(), bubble.x, bubble.y + 34);

    /* POWER and MODE buttons, side by side below the bubble */
    int btn_y = 206;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* power_label = acOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 78;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 162;

    /* Dim the MODE button when powered off, since it's a no-op then */
    uint32_t mode_btn_color = acOn ? TFT_WHITE : 0x666666U;
    uint32_t mode_text_color = acOn ? _theme_color : TFT_WHITE;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, mode_btn_color);
    _canvas->setTextColor(mode_text_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
