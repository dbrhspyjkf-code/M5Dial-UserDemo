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


void GUI_SetBrightness::renderPage(bool brightnessMode, int brightnessPct,
                                    const std::string& effectName, bool lightOn)
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

    /* Small mode indicator, above the big value */
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(brightnessMode ? "BRIGHTNESS" : "EFFECT", bubble.x, bubble.y - 68);

    /* Big value: brightness percent, or the current effect name */
    _canvas->setTextSize(brightnessMode ? 3 : 2);
    if (brightnessMode)
    {
        char string_buffer[24];
        snprintf(string_buffer, sizeof(string_buffer), "%d%%", brightnessPct);
        _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 56);
    }
    else
    {
        std::string display_effect = effectName;
        while (_canvas->textWidth(display_effect.c_str()) > 200 && display_effect.size() > 3)
        {
            display_effect = display_effect.substr(0, display_effect.size() - 4) + "...";
        }
        _canvas->drawCenterString(display_effect.c_str(), bubble.x, bubble.y - 50);
    }

    _canvas->setTextSize(1);
    _canvas->drawCenterString("FISH LIGHT", bubble.x, bubble.y + 26);

    /* MODE and ON/OFF buttons, side by side below the bubble (bubble bottom
       edge is at bubble.y + bubble.height/2 = 190) */
    int btn_y = 202;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 75;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    const char* power_label = lightOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 165;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
