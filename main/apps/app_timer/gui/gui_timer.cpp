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

    /* "TIMER" label, directly on the themed background (no bubble) */
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("TIMER", 120, 45);

    /* Digits (MM:SS), directly on the themed background (no box), centered
       on screen. Blanked when showDigits is false (DONE-state blink).
       Vertical position is computed from the actual measured font height
       (fontHeight()) instead of a guessed offset — guessed offsets kept
       being wrong for this font/size combination. */
    int digit_x = 120;
    int digit_center_y = 115;

    _canvas->setTextSize(3);
    int digit_h = _canvas->fontHeight();
    int digit_top_y = digit_center_y - digit_h / 2;

    if (showDigits)
    {
        char mm_buf[3];
        char ss_buf[3];
        snprintf(mm_buf, sizeof(mm_buf), "%02d", displayMinutes);
        snprintf(ss_buf, sizeof(ss_buf), "%02d", displaySeconds);

        _canvas->setTextColor(TFT_WHITE);

        _canvas->drawRightString(mm_buf, digit_x - 10, digit_top_y);
        _canvas->drawCenterString(":", digit_x, digit_top_y);
        _canvas->drawString(ss_buf, digit_x + 10, digit_top_y);
    }

    /* Pill button (START / PAUSE / RESUME / RESET), positioned with a fixed
       gap below the digit row's actual (measured) bottom edge. */
    int pill_x = 120;
    int pill_height = 28;
    int pill_y = digit_top_y + digit_h + 24 + pill_height / 2;

    _canvas->setTextSize(1);
    int text_w = _canvas->textWidth(pillLabel.c_str());
    int text_h = _canvas->fontHeight();
    int pill_width = text_w + 36;
    if (pill_width < 100) pill_width = 100;

    _canvas->fillSmoothRoundRect(pill_x - pill_width / 2, pill_y - pill_height / 2,
                                  pill_width, pill_height, pill_height / 2, TFT_WHITE);
    _canvas->setTextColor(TFT_BLACK);
    _canvas->drawCenterString(pillLabel.c_str(), pill_x, pill_y - text_h / 2);

    /* Quit indicator */
    _draw_quit_button();

    _canvas->pushSprite(0, 0);
}
