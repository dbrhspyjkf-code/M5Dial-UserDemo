/**
 * @file gui_reachy.cpp
 */
#include "gui_reachy.h"
#include "../../common_define.h"
#include <cstdio>
#include <vector>

static size_t _next_utf8_char(const std::string& s, size_t pos)
{
    if (pos >= s.size()) return pos;
    unsigned char c = (unsigned char)s[pos];
    if ((c & 0x80) == 0) return pos + 1;
    if ((c & 0xE0) == 0xC0) return pos + 2;
    if ((c & 0xF0) == 0xE0) return pos + 3;
    if ((c & 0xF8) == 0xF0) return pos + 4;
    return pos + 1;
}

static std::vector<std::string> _wrap(LGFX_Sprite* canvas, const std::string& text,
                                      int max_width, int max_lines)
{
    std::vector<std::string> lines;
    std::string line;
    for (size_t pos = 0; pos < text.size();)
    {
        size_t next = _next_utf8_char(text, pos);
        std::string candidate = line + text.substr(pos, next - pos);
        if (!line.empty() && canvas->textWidth(candidate.c_str()) > max_width)
        {
            lines.push_back(line);
            line.clear();
            if ((int)lines.size() >= max_lines) break;
            continue;
        }
        line = std::move(candidate);
        pos = next;
    }
    if (!line.empty() && (int)lines.size() < max_lines)
        lines.push_back(line);

    if ((int)lines.size() == max_lines && canvas->textWidth(lines.back().c_str()) > max_width - 12)
        lines.back() += "...";
    return lines;
}

void GUI_Reachy::init()
{
    renderChat("", "");
}

void GUI_Reachy::renderPage(const std::string& status,
                            const std::string& user,
                            const std::string& assistant)
{
    (void)status;
    renderChat(user, assistant);
}

static void _draw_row(LGFX_Sprite* canvas, int y, const char* label,
                      const char* value, bool selected)
{
    uint16_t bg = selected ? 0x3a8f : 0x18c3;
    canvas->fillSmoothRoundRect(24, y, 192, 36, 8, bg);
    canvas->setFont(GUI_FONT_CN_SMALL);
    canvas->setTextSize(1);
    canvas->setTextColor(selected ? TFT_WHITE : 0xbdf7);
    canvas->drawString(label, 38, y + 8);
    canvas->drawRightString(value, 202, y + 8);
}

static void _draw_edge_progress(LGFX_Sprite* canvas, int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int end = -90 + (360 * percent) / 100;
    canvas->fillArc(120, 120, 112, 119, -90, 270, 0x2104);
    if (percent >= 100)
        canvas->fillArc(120, 120, 112, 119, -90, 270, 0x07ff);
    else if (percent > 0)
        canvas->fillArc(120, 120, 112, 119, -90, end, 0x07ff);
}

void GUI_Reachy::renderChat(const std::string& user,
                            const std::string& assistant,
                            int volume_percent,
                            int mic_flash)
{
    _canvas->fillScreen(TFT_BLACK);
    _icon->pushRotateZoom(_canvas, _canvas_half_width, 27, 0, 1.2, 1.2, TFT_BLACK);

    _canvas->setFont(&fonts::Font0);
    _canvas->setTextSize(1);
    _canvas->setTextColor(0x9CF3);
    _canvas->drawString("YOU", 36, 62);

    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setTextColor(TFT_WHITE);
    auto user_lines = _wrap(_canvas, user.empty() ? "-" : user, 172, 3);
    int y = 82;
    for (auto& line : user_lines)
    {
        _canvas->drawString(line.c_str(), 36, y);
        y += 20;
    }

    _canvas->setFont(&fonts::Font0);
    _canvas->setTextColor(0xA7E0);
    _canvas->drawString("AI", 36, 128);

    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setTextColor(TFT_WHITE);
    auto assistant_lines = _wrap(_canvas, assistant.empty() ? "-" : assistant, 172, 3);
    y = 145;
    for (auto& line : assistant_lines)
    {
        _canvas->drawString(line.c_str(), 36, y);
        y += 20;
    }

    if (volume_percent >= 0)
        _draw_edge_progress(_canvas, volume_percent);

    if (mic_flash != 0)
    {
        _canvas->setFont(&fonts::Font0);
        _canvas->setTextSize(1);
        _canvas->setTextColor(mic_flash == 1 ? TFT_GREEN : TFT_RED);
        _canvas->drawString(mic_flash == 1 ? "MIC ON" : "MIC OFF", 158, 62);
    }

    _draw_quit_button(0x8410);
    _canvas->pushSprite(0, 0);
}

void GUI_Reachy::renderAudio(const REACHY_CLIENT::AudioState& audio,
                             int edit_mode)
{
    _canvas->fillScreen(TFT_BLACK);
    _icon->pushRotateZoom(_canvas, _canvas_half_width, 27, 0, 1.2, 1.2, TFT_BLACK);

    char volume[24];
    if (audio.volume_percent >= 0)
        snprintf(volume, sizeof(volume), "%d%%", audio.volume_percent);
    else
        snprintf(volume, sizeof(volume), "--");

    char mic[8];
    snprintf(mic, sizeof(mic), "%s", audio.mic_enabled ? "ON" : "OFF");

    char vad[24];
    snprintf(vad, sizeof(vad), "%.3f", audio.vad_rms_min);

    _draw_row(_canvas, 58, "Volume", volume, edit_mode == 1);
    _draw_row(_canvas, 103, "MIC enable", mic, false);
    _draw_row(_canvas, 148, "VAD", vad, edit_mode == 2);

    if (edit_mode == 1)
        _draw_edge_progress(_canvas, audio.volume_percent);
    else if (edit_mode == 2)
        _draw_edge_progress(_canvas, (int)((audio.vad_rms_min - 0.001f) * 100.0f / 0.499f));

    _draw_quit_button(0x8410);
    _canvas->pushSprite(0, 0);
}

void GUI_Reachy::renderSystem(const std::string& status)
{
    _canvas->fillScreen(TFT_BLACK);
    _icon->pushRotateZoom(_canvas, _canvas_half_width, 27, 0, 1.2, 1.2, TFT_BLACK);
    _draw_row(_canvas, 58, "Restart", "YRobot", false);
    _draw_row(_canvas, 103, "Restart", "Reachy", false);
    _draw_row(_canvas, 148, "Shutdown", "hold", false);

    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setTextColor(0x8410);
    _canvas->drawCenterString(status.c_str(), 120, 192);
    _draw_quit_button(0x8410);
    _canvas->pushSprite(0, 0);
}

void GUI_Reachy::renderVideo(const REACHY_CLIENT::VideoState& video,
                             const REACHY_CLIENT::JpegFrame& frame,
                             const std::string& status)
{
    _canvas->fillScreen(TFT_BLACK);
    _icon->pushRotateZoom(_canvas, _canvas_half_width, 27, 0, 1.2, 1.2, TFT_BLACK);

    static const int preview_x = 24;
    static const int preview_y = 54;
    static const int preview_w = 192;
    static const int preview_h = 112;
    _canvas->fillSmoothRoundRect(preview_x, preview_y, preview_w, preview_h, 8, 0x18c3);
    if (frame.ok && !frame.bytes.empty())
    {
        _canvas->drawJpg(frame.bytes.data(), frame.bytes.size(), preview_x + 6, preview_y + 6,
                         preview_w - 12, preview_h - 12, 0, 0, -1.0f, -1.0f);
    }

    if (!frame.ok)
    {
        _canvas->setFont(GUI_FONT_CN_SMALL);
        _canvas->setTextSize(1);
        _canvas->setTextColor(0x8410);
        _canvas->drawCenterString("preview waiting", 120, 112);
    }

    char timing[40];
    snprintf(timing, sizeof(timing), "active %.1fs idle %.1fs", video.active_s, video.idle_s);
    _canvas->setTextColor(0x8410);
    _canvas->drawCenterString(timing, 120, 194);
    _canvas->drawCenterString(status.c_str(), 120, 214);

    _draw_quit_button(0x8410);
    _canvas->pushSprite(0, 0);
}

void GUI_Reachy::renderMode(const REACHY_CLIENT::ModeState& mode,
                            int edit_mode,
                            bool restart_confirm,
                            const std::string& status)
{
    _canvas->fillScreen(TFT_BLACK);
    _icon->pushRotateZoom(_canvas, _canvas_half_width, 27, 0, 1.2, 1.2, TFT_BLACK);

    const char* backend = mode.configured_backend == "qwen" ? "QWEN" : "XIAOZHI";
    const char* voice = mode.configured_backend == "qwen" ?
                        (mode.configured_voice.empty() ? "--" : mode.configured_voice.c_str()) :
                        "Disable";

    _draw_row(_canvas, 58, "Backend", backend, edit_mode == 1);
    _draw_row(_canvas, 103, "Voice", voice, edit_mode == 2 && mode.configured_backend == "qwen");
    _draw_row(_canvas, 148, "Apply", "restart", false);

    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setTextColor(0x8410);
    _canvas->drawCenterString(status.c_str(), 120, 194);

    if (restart_confirm)
    {
        _canvas->fillSmoothRoundRect(28, 76, 184, 92, 10, 0x2104);
        _canvas->drawCenterString("Restart now?", 120, 92);
        _canvas->fillSmoothRoundRect(46, 126, 58, 34, 8, 0x18c3);
        _canvas->fillSmoothRoundRect(136, 126, 58, 34, 8, 0x3a8f);
        _canvas->setTextColor(TFT_WHITE);
        _canvas->drawCenterString("NO", 75, 136);
        _canvas->drawCenterString("YES", 165, 136);
    }

    _draw_quit_button(0x8410);
    _canvas->pushSprite(0, 0);
}
