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

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), 120, 115 - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), 120, 150);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_MoreMenu::renderPage(const std::string& name, float price, float absChg, float chg, int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    /* Stock name, directly on the themed background (no bubble),
       truncated with "..." if it doesn't fit */
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    std::string display_name = name;
    while (_canvas->textWidth(display_name.c_str()) > 200 && display_name.size() > 3)
    {
        display_name = display_name.substr(0, display_name.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_name.c_str(), 120, 62);

    /* Price */
    char price_buffer[24];
    snprintf(price_buffer, sizeof(price_buffer), "%.2f", price);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(price_buffer, 120, 100);

    /* Absolute change + percentage change, colored - red for up, green
       for down (A-share convention) */
    char chg_buffer[32];
    snprintf(chg_buffer, sizeof(chg_buffer), "%+.2f  %+.2f%%", absChg, chg);
    /* TFT_RED is LovyanGFX's RGB565 constant (0xF800), not a 24-bit RGB
       value like every other color in this codebase - passing it here
       would get reinterpreted as green, not red. Use an explicit 24-bit
       red instead. */
    uint32_t chg_color = (chg >= 0) ? 0xE53935U : 0x00B050U;
    _canvas->setTextColor(chg_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(chg_buffer, 120, 150);

    /* Position indicator, e.g. "3 / 11" */
    char position_buffer[16];
    snprintf(position_buffer, sizeof(position_buffer), "%d / %d", index + 1, count);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(position_buffer, 120, 190);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_MoreMenu::renderAnalysis(const std::string& name, const std::string& oneSentence,
                                    const std::string& summary, int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    /* Analysis text (one_sentence/analysis_summary) is Chinese - use the
       CJK-capable font throughout this page, same reason as the EMAIL
       app's sender/subject (Font0, this project's default, is
       ASCII-only and shows missing-glyph boxes for Chinese). */
    _canvas->setFont(&fonts::efontCN_16_b);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);

    /* Stock name header */
    std::string display_name = name;
    while (_canvas->textWidth(display_name.c_str()) > 200 && display_name.size() > 3)
    {
        display_name = display_name.substr(0, display_name.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_name.c_str(), 120, 48);

    /* Body text: prefer the fuller analysis_summary, fall back to the
       one-line takeaway, or a placeholder if the server has neither */
    std::string body = !summary.empty() ? summary : oneSentence;
    if (body.empty())
    {
        body = "暂无分析摘要";
    }

    /* Wrapped onto up to 6 lines - this page doesn't scroll, so
       anything past that is simply cut off with "..." */
    int line_y = 70;
    for (int line = 0; line < 6 && !body.empty(); line++)
    {
        size_t fit = body.size();
        while (fit > 0 && _canvas->textWidth(body.substr(0, fit).c_str()) > 210)
        {
            fit--;
        }
        if (fit == 0) break;

        std::string this_line = body.substr(0, fit);
        body = body.substr(fit);

        if (!body.empty() && line == 5)
        {
            while (this_line.size() > 3 && _canvas->textWidth((this_line + "...").c_str()) > 210)
            {
                this_line = this_line.substr(0, this_line.size() - 1);
            }
            this_line += "...";
        }

        _canvas->drawCenterString(this_line.c_str(), 120, line_y);
        line_y += 17;
    }

    _canvas->setFont(&fonts::Font0);

    /* Position indicator, e.g. "3 / 11" */
    char position_buffer[16];
    snprintf(position_buffer, sizeof(position_buffer), "%d / %d", index + 1, count);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(position_buffer, 120, 190);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
