/**
 * @file gui_lcd_test.cpp
 */
#include "gui_lcd_test.h"
#include <cstdio>


void GUI_LCD_TEST::init()
{
}


void GUI_LCD_TEST::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_LCD_TEST::renderVolumePage(int volumePct, bool tvOn)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("VOLUME", 120, 60);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", volumePct);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, 120, 100);

    _canvas->setTextSize(1);
    _canvas->drawCenterString("TV", 120, 148);

    /* MODE and POWER buttons, side by side */
    int btn_y = 206;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 78;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    const char* power_label = tvOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 162;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_LCD_TEST::renderNavPage()
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("NAV", 120, 50);

    /* D-pad cross, centered around (120, 110) */
    auto draw_dpad_button = [this](const char* label, int x, int y, int w, int h)
    {
        _canvas->fillSmoothRoundRect(x - w / 2, y - h / 2, w, h, 8, TFT_WHITE);
        _canvas->setTextColor(_theme_color);
        int text_h = _canvas->fontHeight();
        _canvas->drawCenterString(label, x, y - text_h / 2);
    };

    draw_dpad_button("^", 120, 84, 34, 24);
    draw_dpad_button("v", 120, 136, 34, 24);
    draw_dpad_button("<", 84, 110, 32, 24);
    draw_dpad_button(">", 156, 110, 32, 24);
    draw_dpad_button("OK", 120, 110, 34, 24);

    /* BACK / MENU / VOL row */
    draw_dpad_button("BACK", 56, 193, 52, 24);
    draw_dpad_button("MENU", 120, 193, 52, 24);
    draw_dpad_button("VOL", 184, 193, 52, 24);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
