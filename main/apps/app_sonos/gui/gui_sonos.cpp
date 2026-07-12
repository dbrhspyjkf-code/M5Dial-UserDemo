/**
 * @file gui_sonos.cpp
 */
#include "gui_sonos.h"
#include <cstdio>


void GUI_Sonos::init()
{
}


void GUI_Sonos::renderStatus(const std::string& line1, const std::string& line2)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("SONOS", 120, 55);

    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), 120, 110 - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), 120, 140);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_Sonos::renderPlaying(const std::string& title, const std::string& artist,
                               bool isPlaying, int volumePercent)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("SONOS", 120, 50);

    /* Title, smaller font, truncated with "..." if it doesn't fit in ~200px */
    _canvas->setTextSize(1);
    std::string display_title = title.empty() ? "(nothing playing)" : title;
    while (_canvas->textWidth(display_title.c_str()) > 200 && display_title.size() > 3)
    {
        display_title = display_title.substr(0, display_title.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_title.c_str(), 120, 85);

    /* Artist, smaller font (GUI_FONT_CN_SMALL instead of the default
       GUI_FONT_CN_BIG — still CJK-capable, unlike LovyanGFX's tiny ASCII-only
       Font0), below the title with more clearance */
    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    std::string display_artist = artist;
    while (_canvas->textWidth(display_artist.c_str()) > 200 && display_artist.size() > 3)
    {
        display_artist = display_artist.substr(0, display_artist.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_artist.c_str(), 120, 122);
    _canvas->setFont(GUI_FONT_CN_BIG);

    /* Prev / Play-Pause / Next buttons */
    struct BtnSpec { int x; const char* label; };
    BtnSpec buttons[3] = {
        {55,  "<<"},
        {120, isPlaying ? "||" : ">"},
        {185, ">>"},
    };

    int btn_y = 170;
    int btn_height = 32;

    for (int i = 0; i < 3; i++)
    {
        _canvas->setTextSize(1);
        int text_w = _canvas->textWidth(buttons[i].label);
        int text_h = _canvas->fontHeight();
        int btn_width = text_w + 30;
        if (btn_width < 44) btn_width = 44;

        _canvas->fillSmoothRoundRect(buttons[i].x - btn_width / 2, btn_y - btn_height / 2,
                                      btn_width, btn_height, btn_height / 2, TFT_WHITE);
        _canvas->setTextColor(TFT_BLACK);
        _canvas->drawCenterString(buttons[i].label, buttons[i].x, btn_y - text_h / 2);
    }

    /* Volume readout, below the buttons */
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "VOL %d%%", volumePercent);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(vol_buf, 120, 198);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
