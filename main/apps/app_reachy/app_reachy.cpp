/**
 * @file app_reachy.cpp
 */
#include "app_reachy.h"
#include "reachy_config.h"
#include "../common_define.h"

using namespace MOONCAKE::USER_APP;

static const uint32_t POLL_MS = 800;
static const uint32_t AUDIO_POLL_MS = 2500;
static const uint32_t VIDEO_POLL_MS = 3000;
static const uint32_t FRAME_POLL_MS = 1500;
static const uint32_t CONTROL_DEBOUNCE_MS = 300;

void AppReachy::onSetup()
{
    setAppName("Reachy");
    setAllowBgRunning(false);
    _data.hal = (HAL::HAL*)getUserData();
}

void AppReachy::onCreate()
{
    _log("onCreate");
    _prepare_frame();
    _render();
    _fetch();
    _fetch_audio();
    _fetch_video();
    _fetch_mode();
    _render();
}

void AppReachy::_fetch()
{
    _data.turn = REACHY_CLIENT::fetch_recent_turn(REACHY_BASE_URL, 200);
    _data.last_fetch_ms = millis();
    _data.status = _data.turn.ok ? "updated" : "no chat";
    if (_data.turn.ok)
        _log("fetch ok user=%d assistant=%d",
             (int)_data.turn.user.size(),
             (int)_data.turn.assistant.size());
    else
        _log("fetch empty");
}

void AppReachy::_fetch_audio()
{
    _data.audio = REACHY_CLIENT::fetch_audio_state(REACHY_BASE_URL);
    _data.last_audio_fetch_ms = millis();
    if (_data.audio.ok)
        _log("audio ok volume=%d mic=%d vad=%.3f",
             _data.audio.volume_percent,
             _data.audio.mic_enabled ? 1 : 0,
             _data.audio.vad_rms_min);
    else
        _log("audio empty");
}

void AppReachy::_fetch_video()
{
    _data.video = REACHY_CLIENT::fetch_video_state(REACHY_BASE_URL);
    _data.last_video_fetch_ms = millis();
    if (_data.video.ok)
        _log("video ok enabled=%d", _data.video.enabled ? 1 : 0);
    else
        _log("video empty");
}

void AppReachy::_prepare_frame()
{
    if (REACHY_CLIENT::prepare_camera_jpeg_buffer(_data.camera_frame))
        _log("frame buffer ready capacity=%d", (int)_data.camera_frame.bytes.capacity());
    else
        _log("frame buffer unavailable");
}

void AppReachy::_fetch_frame()
{
    REACHY_CLIENT::fetch_camera_jpeg(REACHY_BASE_URL, _data.camera_frame);
    _data.last_frame_fetch_ms = millis();
    _data.status = _data.camera_frame.ok ? "frame ok" : "frame fail";
    _log("frame %s bytes=%d", _data.camera_frame.ok ? "ok" : "fail",
         (int)_data.camera_frame.bytes.size());
}

void AppReachy::_fetch_mode()
{
    _data.mode = REACHY_CLIENT::fetch_mode_state(REACHY_BASE_URL);
    if (_data.mode.ok)
        _log("mode ok backend=%s voice=%s",
             _data.mode.configured_backend.c_str(),
             _data.mode.configured_voice.c_str());
    else
        _log("mode empty");
}

void AppReachy::_render()
{
    switch (_data.page)
    {
        case REACHY::CHAT:
            _gui.renderChat(_data.turn.user, _data.turn.assistant,
                            _data.volume_dirty ? _data.audio.volume_percent : -1);
            break;
        case REACHY::AUDIO:
            _gui.renderAudio(_data.audio, _data.audio_edit);
            break;
        case REACHY::SYSTEM:
            _gui.renderSystem(_data.status);
            break;
        case REACHY::MODE:
            _gui.renderMode(_data.mode, _data.mode_edit, _data.restart_confirm, _data.status);
            break;
        case REACHY::VIDEO:
            _gui.renderVideo(_data.video, _data.camera_frame, _data.status);
            break;
        default:
            break;
    }
}

void AppReachy::_next_page(int direction)
{
    _data.audio_edit = REACHY::EDIT_NONE;
    _data.mode_edit = REACHY::MODE_EDIT_NONE;
    _data.restart_confirm = false;
    int next = ((int)_data.page + direction + (int)REACHY::PAGE_COUNT) % (int)REACHY::PAGE_COUNT;
    _data.page = (REACHY::Page)next;
    if (_data.page == REACHY::AUDIO)
        _fetch_audio();
    else if (_data.page == REACHY::MODE)
        _fetch_mode();
    else if (_data.page == REACHY::VIDEO)
    {
        _fetch_video();
        _fetch_frame();
    }
    _render();
}

void AppReachy::_commit_audio_changes()
{
    if (_data.volume_dirty && millis() - _data.last_audio_change_ms >= CONTROL_DEBOUNCE_MS)
    {
        auto result = REACHY_CLIENT::set_volume(REACHY_BASE_URL, _data.audio.volume_percent);
        _data.status = result.ok ? "volume ok" : "volume fail";
        _data.volume_dirty = false;
    }
    if (_data.vad_dirty && millis() - _data.last_audio_change_ms >= CONTROL_DEBOUNCE_MS)
    {
        auto result = REACHY_CLIENT::set_vad(REACHY_BASE_URL, _data.audio.vad_rms_min);
        _data.status = result.ok ? "vad ok" : "vad fail";
        _data.vad_dirty = false;
    }
}

void AppReachy::_adjust_volume(int direction)
{
    if (_data.audio.volume_percent < 0) _data.audio.volume_percent = 0;
    _data.audio.volume_percent += direction * 2;
    if (_data.audio.volume_percent < 0) _data.audio.volume_percent = 0;
    if (_data.audio.volume_percent > 100) _data.audio.volume_percent = 100;
    _data.volume_dirty = true;
    _data.last_audio_change_ms = millis();
}

void AppReachy::_ensure_mic_enabled()
{
    if (_data.audio.mic_enabled)
        return;
    _data.audio.mic_enabled = true;
    auto result = REACHY_CLIENT::set_mic_enabled(REACHY_BASE_URL, true);
    _data.status = result.ok ? "mic ok" : "mic fail";
}

void AppReachy::_restart_yrobot()
{
    auto result = REACHY_CLIENT::restart_yrobot(REACHY_BASE_URL);
    _data.status = result.ok ? "restart sent" : "restart fail";
    _render();
}

void AppReachy::_toggle_mic()
{
    _data.audio.mic_enabled = !_data.audio.mic_enabled;
    auto result = REACHY_CLIENT::set_mic_enabled(REACHY_BASE_URL, _data.audio.mic_enabled);
    _data.status = result.ok ? "mic ok" : "mic fail";
    _render();
}

void AppReachy::_toggle_video()
{
    _data.video.enabled = !_data.video.enabled;
    auto result = REACHY_CLIENT::set_video_enabled(REACHY_BASE_URL, _data.video.enabled);
    _data.status = result.ok ? "video ok" : "video fail";
    _fetch_video();
    _fetch_frame();
    _render();
}

void AppReachy::_cycle_backend(int direction)
{
    (void)direction;
    if (_data.mode.configured_backend == "qwen")
        _data.mode.configured_backend = "xiaozhi";
    else
        _data.mode.configured_backend = "qwen";
}

void AppReachy::_cycle_voice(int direction)
{
    if (_data.mode.configured_backend != "qwen" || _data.mode.available_voices.empty())
        return;

    int current = 0;
    for (int i = 0; i < (int)_data.mode.available_voices.size(); i++)
    {
        if (_data.mode.available_voices[i] == _data.mode.configured_voice)
        {
            current = i;
            break;
        }
    }
    int next = (current + direction + (int)_data.mode.available_voices.size()) %
               (int)_data.mode.available_voices.size();
    _data.mode.configured_voice = _data.mode.available_voices[next];
}

void AppReachy::_apply_mode()
{
    auto backend = REACHY_CLIENT::set_backend(REACHY_BASE_URL, _data.mode.configured_backend.c_str());
    bool ok = backend.ok;
    if (_data.mode.configured_backend == "qwen" && !_data.mode.configured_voice.empty())
    {
        auto voice = REACHY_CLIENT::set_voice(REACHY_BASE_URL, _data.mode.configured_voice.c_str());
        ok = ok && voice.ok;
    }
    _data.status = ok ? "mode saved" : "mode fail";
    _data.restart_confirm = true;
    _render();
}

void AppReachy::_handle_encoder()
{
    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;
    if (_data.page == REACHY::CHAT)
    {
        _ensure_mic_enabled();
        _adjust_volume(direction);
        _render();
        return;
    }

    if (_data.page == REACHY::AUDIO && _data.audio_edit != REACHY::EDIT_NONE)
    {
        if (_data.audio_edit == REACHY::EDIT_VOLUME)
        {
            _adjust_volume(direction);
        }
        else if (_data.audio_edit == REACHY::EDIT_VAD)
        {
            _data.audio.vad_rms_min += direction * 0.005f;
            if (_data.audio.vad_rms_min < 0.001f) _data.audio.vad_rms_min = 0.001f;
            if (_data.audio.vad_rms_min > 0.5f) _data.audio.vad_rms_min = 0.5f;
            _data.vad_dirty = true;
            _data.last_audio_change_ms = millis();
        }
        _render();
        return;
    }

    if (_data.page == REACHY::MODE)
    {
        if (_data.mode_edit == REACHY::MODE_EDIT_BACKEND)
            _cycle_backend(direction);
        else if (_data.mode_edit == REACHY::MODE_EDIT_VOICE)
            _cycle_voice(direction);
        _render();
    }
}

void AppReachy::_handle_touch()
{
    bool touched = _data.hal->tp.isTouched();
    if (!touched)
    {
        _data.touch_was_down = false;
        return;
    }
    if (_data.touch_was_down)
        return;
    _data.touch_was_down = true;

    _data.hal->tp.update();
    int x = _data.hal->tp.getTouchPointBuffer().x;
    int y = _data.hal->tp.getTouchPointBuffer().y;

    if (x < 55)
    {
        _next_page(1);
        return;
    }
    if (x > 185)
    {
        _next_page(-1);
        return;
    }

    if (_data.restart_confirm)
    {
        if (_data.page == REACHY::MODE && y >= 124 && y <= 170)
        {
            if (x >= 120)
                _restart_yrobot();
            else
            {
                _data.restart_confirm = false;
                _render();
            }
        }
        return;
    }

    if (_data.page == REACHY::AUDIO)
    {
        if (y >= 58 && y <= 94)
            _data.audio_edit = REACHY::EDIT_VOLUME;
        else if (y >= 103 && y <= 139)
            _toggle_mic();
        else if (y >= 148 && y <= 184)
            _data.audio_edit = REACHY::EDIT_VAD;
        _render();
    }
    else if (_data.page == REACHY::SYSTEM)
    {
        if (y >= 58 && y <= 94)
            _restart_yrobot();
        else if (y >= 148 && y <= 184)
        {
            _data.status = "shutdown locked";
            _render();
        }
    }
    else if (_data.page == REACHY::VIDEO)
    {
        if (y >= 66 && y <= 144)
            _toggle_video();
    }
    else if (_data.page == REACHY::MODE)
    {
        if (y >= 58 && y <= 94)
            _data.mode_edit = REACHY::MODE_EDIT_BACKEND;
        else if (y >= 103 && y <= 139)
            _data.mode_edit = REACHY::MODE_EDIT_VOICE;
        else if (y >= 148 && y <= 184)
            _apply_mode();
        _render();
    }
}

void AppReachy::onRunning()
{
    _handle_encoder();
    _handle_touch();
    _commit_audio_changes();

    if (_data.page == REACHY::CHAT &&
        (_data.last_fetch_ms == 0 || millis() - _data.last_fetch_ms > POLL_MS))
    {
        _fetch();
        _render();
    }
    else if (_data.page == REACHY::AUDIO &&
             !_data.volume_dirty && !_data.vad_dirty &&
             (_data.last_audio_fetch_ms == 0 || millis() - _data.last_audio_fetch_ms > AUDIO_POLL_MS))
    {
        _fetch_audio();
        _render();
    }
    else if (_data.page == REACHY::VIDEO &&
             (_data.last_video_fetch_ms == 0 || millis() - _data.last_video_fetch_ms > VIDEO_POLL_MS))
    {
        _fetch_video();
        _render();
    }
    else if (_data.page == REACHY::VIDEO &&
             (_data.last_frame_fetch_ms == 0 || millis() - _data.last_frame_fetch_ms > FRAME_POLL_MS))
    {
        _fetch_frame();
        _render();
    }

    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);
        if (_data.audio_edit != REACHY::EDIT_NONE)
        {
            _data.audio_edit = REACHY::EDIT_NONE;
            _render();
            return;
        }
        if (_data.restart_confirm)
        {
            _data.restart_confirm = false;
            _render();
            return;
        }
        if (_data.mode_edit != REACHY::MODE_EDIT_NONE)
        {
            _data.mode_edit = REACHY::MODE_EDIT_NONE;
            _render();
            return;
        }
        destroyApp();
    }
}

void AppReachy::onDestroy()
{
    _log("onDestroy");
    _data.hal->canvas->setFont(&fonts::Font0);
    _data.hal->canvas->setTextSize(1);
}
