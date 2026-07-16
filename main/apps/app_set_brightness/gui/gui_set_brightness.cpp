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


static void _draw_position_indicator(LGFX_Sprite* canvas, int index, int count, int y)
{
    canvas->setFont(&fonts::Font0);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    char position_buf[16];
    snprintf(position_buf, sizeof(position_buf), "%d / %d", index + 1, count);
    canvas->drawCenterString(position_buf, 120, y);
}


void GUI_SetBrightness::renderSwitch(const std::string& name, bool isOn, int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    /* Name (Chinese) */
    _canvas->setFont(GUI_FONT_CN_BIG);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    std::string display_name = name;
    while (_canvas->textWidth(display_name.c_str()) > 200 && display_name.size() > 3)
    {
        display_name = display_name.substr(0, display_name.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_name.c_str(), 120, 55);

    /* Big ON/OFF button fills most of the middle of the screen - color
       itself signals state (green = on, gray = off) so it reads at a
       glance without needing to parse the label text */
    uint32_t btn_color = isOn ? 0x03A964U : 0x666666U;
    _canvas->fillSmoothRoundRect(60, 95, 120, 60, 20, btn_color);

    _canvas->setFont(&fonts::Font0);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    int text_h = _canvas->fontHeight();
    _canvas->drawCenterString(isOn ? "ON" : "OFF", 120, 125 - text_h / 2);

    _draw_position_indicator(_canvas, index, count, 190);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_SetBrightness::renderMasterBedroom(int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setFont(GUI_FONT_CN_BIG);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("主人房灯", 120, 55);

    /* Two independent, stateless toggle buttons - HA reports these as
       "select" actuators that always read "unknown" (a physical wall
       switch panel exposed as a fire-and-forget toggle, not a switch
       domain entity with a real on/off state), so there's no state to
       color or label these with beyond their names. */
    _canvas->fillSmoothRoundRect(20, 95, 98, 60, 20, TFT_WHITE);
    _canvas->fillSmoothRoundRect(122, 95, 98, 60, 20, TFT_WHITE);

    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    int text_h = _canvas->fontHeight();
    _canvas->drawCenterString("大灯", 69, 125 - text_h / 2);
    _canvas->drawCenterString("小灯", 171, 125 - text_h / 2);

    _draw_position_indicator(_canvas, index, count, 190);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_SetBrightness::renderFishtank(bool brightnessMode, int brightnessPct, const std::string& effectName,
                                        bool lightOn, int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setFont(&fonts::Font0);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(brightnessMode ? "BRIGHTNESS" : "EFFECT", 120, 55);

    /* "-" / "+" buttons flanking the value, adjusting brightness or
       cycling effect - the encoder is used for browsing between lights
       in this app, so this page needs its own value-adjustment
       controls instead of the encoder like before. */
    _canvas->fillSmoothRoundRect(20, 100, 45, 40, 14, TFT_WHITE);
    _canvas->fillSmoothRoundRect(175, 100, 45, 40, 14, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(2);
    int minus_plus_h = _canvas->fontHeight();
    _canvas->drawCenterString("-", 42, 120 - minus_plus_h / 2);
    _canvas->drawCenterString("+", 197, 120 - minus_plus_h / 2);

    /* Big value: brightness percent, or the current effect name (effect
       names are Chinese - Font0 is ASCII-only and shows missing-glyph
       boxes for them, so this branch switches to a CJK-capable font). */
    _canvas->setTextColor(TFT_WHITE);
    if (brightnessMode)
    {
        char string_buffer[24];
        snprintf(string_buffer, sizeof(string_buffer), "%d%%", brightnessPct);
        _canvas->setFont(&fonts::Font0);
        _canvas->setTextSize(3);
        int val_h = _canvas->fontHeight();
        _canvas->drawCenterString(string_buffer, 120, 120 - val_h / 2);
    }
    else
    {
        std::string display_effect = effectName;
        _canvas->setFont(GUI_FONT_CN_BIG);
        _canvas->setTextSize(1);
        while (_canvas->textWidth(display_effect.c_str()) > 130 && display_effect.size() > 3)
        {
            display_effect = display_effect.substr(0, display_effect.size() - 4) + "...";
        }
        int val_h = _canvas->fontHeight();
        _canvas->drawCenterString(display_effect.c_str(), 120, 120 - val_h / 2);
        _canvas->setFont(&fonts::Font0);
    }

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    _canvas->drawCenterString("FISH LIGHT", 120, 148);

    _draw_position_indicator(_canvas, index, count, 172);

    /* MODE and ON/OFF buttons, side by side */
    int btn_y = 198;
    int btn_height = 28;

    _canvas->setTextSize(2);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 74;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    const char* power_label = lightOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 166;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
