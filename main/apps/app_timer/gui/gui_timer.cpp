/**
 * @file gui_timer.cpp
 */
#include "gui_timer.h"
#include <cstdio>


void GUI_Timer::init()
{
}


void GUI_Timer::renderPage(int displayMinutes, int displaySeconds, int selectedField,
                            const std::string& pillLabel, bool showDigits)
{
    _canvas->fillScreen(_theme_color);

    /* Icon */
    _draw_top_icon();

    /* Bubble */
    BasicObeject_t bubble;
    bubble.x = 120;
    bubble.y = 105;
    bubble.width = 240;
    bubble.height = 110;
    _canvas->fillSmoothRoundRect(bubble.x - bubble.width / 2, bubble.y - bubble.height / 2,
                                  bubble.width, bubble.height, 30, TFT_WHITE);

    /* "TIMER" label */
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("TIMER", bubble.x, bubble.y - 34);

    /* Digits (MM:SS), blanked when showDigits is false (DONE-state blink) */
    if (showDigits)
    {
        char mm_buf[3];
        char ss_buf[3];
        snprintf(mm_buf, sizeof(mm_buf), "%02d", displayMinutes);
        snprintf(ss_buf, sizeof(ss_buf), "%02d", displaySeconds);

        _canvas->setTextColor(TFT_BLACK);
        _canvas->setTextSize(3);
        _canvas->drawRightString(mm_buf, bubble.x - 10, bubble.y - 10);
        _canvas->drawCenterString(":", bubble.x, bubble.y - 10);
        _canvas->drawString(ss_buf, bubble.x + 10, bubble.y - 10);
    }

    /* Selected-field underline */
    if (selectedField == 0)
    {
        _canvas->fillSmoothRoundRect(70, bubble.y + 8, 40, 4, 2, _theme_color);
    }
    else if (selectedField == 1)
    {
        _canvas->fillSmoothRoundRect(130, bubble.y + 8, 40, 4, 2, _theme_color);
    }

    /* Pill button (START / PAUSE / RESUME / RESET) */
    int pill_x = 120;
    int pill_y = 190;
    int pill_width = 100;
    int pill_height = 30;
    _canvas->fillSmoothRoundRect(pill_x - pill_width / 2, pill_y - pill_height / 2,
                                  pill_width, pill_height, pill_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(pillLabel.c_str(), pill_x, pill_y - 8);

    /* Quit indicator */
    _draw_quit_button();

    _canvas->pushSprite(0, 0);
}
