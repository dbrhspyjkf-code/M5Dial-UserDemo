/**
 * @file gui_ble_server.cpp
 */
#include "gui_ble_server.h"
#include <cstdio>


void GUI_BLE_Server::init()
{
}


void GUI_BLE_Server::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_BLE_Server::renderPage(const std::string& folderName, int unreadCount,
                                 const std::string& sender, const std::string& subject,
                                 int index, int count)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    /* Folder name + unread count. Uses the CJK-capable font throughout
       this page (not just Font0) since folder names/sender/subject can
       all be Chinese - Font0 (this project's default) is ASCII-only
       and would show missing-glyph boxes for any Chinese character. */
    _canvas->setFont(&fonts::efontCN_16_b);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    char folder_buf[48];
    snprintf(folder_buf, sizeof(folder_buf), "%s (%d)", folderName.c_str(), unreadCount);
    std::string display_folder = folder_buf;
    while (_canvas->textWidth(display_folder.c_str()) > 220 && display_folder.size() > 3)
    {
        display_folder = display_folder.substr(0, display_folder.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_folder.c_str(), 120, 55);

    /* Sender */
    std::string display_sender = sender.empty() ? "(unknown)" : sender;
    while (_canvas->textWidth(display_sender.c_str()) > 200 && display_sender.size() > 3)
    {
        display_sender = display_sender.substr(0, display_sender.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_sender.c_str(), 120, 95);

    /* Subject, wrapped onto up to 3 lines */
    std::string remaining = subject.empty() ? "(no subject)" : subject;
    int line_y = 120;
    for (int line = 0; line < 3 && !remaining.empty(); line++)
    {
        size_t fit = remaining.size();
        while (fit > 0 && _canvas->textWidth(remaining.substr(0, fit).c_str()) > 210)
        {
            fit--;
        }
        if (fit == 0) break;

        std::string this_line = remaining.substr(0, fit);
        remaining = remaining.substr(fit);

        if (!remaining.empty() && line == 2)
        {
            while (this_line.size() > 3 && _canvas->textWidth((this_line + "...").c_str()) > 210)
            {
                this_line = this_line.substr(0, this_line.size() - 1);
            }
            this_line += "...";
        }

        _canvas->drawCenterString(this_line.c_str(), 120, line_y);
        line_y += 18;
    }

    /* Position indicator, e.g. "2 / 3" */
    char position_buf[16];
    snprintf(position_buf, sizeof(position_buf), "%d / %d", index + 1, count);
    _canvas->drawCenterString(position_buf, 120, 192);

    _canvas->setFont(&fonts::Font0);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
