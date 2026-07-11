/**
 * @file app_sonos.cpp
 */
#include "app_sonos.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::SONOS;


void AppSonos::onSetup()
{
    setAppName("AppSonos");
    setAllowBgRunning(false);

    SONOS::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void AppSonos::_refresh_state()
{
    HA_CLIENT::MediaPlayerState mp = HA_CLIENT::get_state(
        SONOS_HA_BASE_URL, SONOS_HA_TOKEN, SONOS_ENTITY_ID);

    if (!mp.ok)
    {
        _data.poll_fail_count++;
        if (_data.poll_fail_count >= 3)
        {
            _data.state = State::ERROR;
            _data.error_message = "HA unreachable";
        }
        return;
    }

    _data.poll_fail_count = 0;
    _data.title = mp.title;
    _data.artist = mp.artist;
    _data.is_playing = (mp.state == "playing");
    if (!_data.volume_dirty)
    {
        _data.volume = mp.volume;
    }
}


void AppSonos::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
    _render();

    bool connected = WIFI_CONNECT::connect(SONOS_WIFI_SSID, SONOS_WIFI_PASSWORD, 8000);
    if (!connected)
    {
        _data.state = State::ERROR;
        _data.error_message = "WiFi failed";
        _render();
        return;
    }

    _refresh_state();
    if (_data.state != State::ERROR)
    {
        _data.state = State::POLLING;
    }
    _data.last_poll_ms = millis();
    _render();
}


void AppSonos::_handle_encoder()
{
    if (_data.state != State::POLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    float step = (_data.hal->encoder.getDirection() < 1) ? 0.05f : -0.05f;
    _data.volume += step;
    if (_data.volume < 0.0f) _data.volume = 0.0f;
    if (_data.volume > 1.0f) _data.volume = 1.0f;

    _data.volume_dirty = true;
    _data.last_volume_change_ms = millis();

    _render();
}


void AppSonos::_handle_volume_debounce()
{
    if (!_data.volume_dirty)
        return;

    if (millis() - _data.last_volume_change_ms < 400)
        return;

    HA_CLIENT::set_volume(SONOS_HA_BASE_URL, SONOS_HA_TOKEN, SONOS_ENTITY_ID, _data.volume);
    _data.volume_dirty = false;
}


void AppSonos::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    _data.hal->tp.update();
    int x = _data.hal->tp.getTouchPointBuffer().x;
    int y = _data.hal->tp.getTouchPointBuffer().y;

    if (_data.state == State::ERROR)
    {
        _data.state = State::CONNECTING;
        _render();
        WIFI_CONNECT::disconnect();
        onCreate();
    }
    else if (_data.state == State::POLLING && y >= 154 && y <= 186)
    {
        if (x >= 30 && x <= 80)
        {
            HA_CLIENT::call_service(SONOS_HA_BASE_URL, SONOS_HA_TOKEN,
                                     "media_player", "media_previous_track", SONOS_ENTITY_ID);
        }
        else if (x >= 95 && x <= 145)
        {
            HA_CLIENT::call_service(SONOS_HA_BASE_URL, SONOS_HA_TOKEN,
                                     "media_player", "media_play_pause", SONOS_ENTITY_ID);
        }
        else if (x >= 160 && x <= 210)
        {
            HA_CLIENT::call_service(SONOS_HA_BASE_URL, SONOS_HA_TOKEN,
                                     "media_player", "media_next_track", SONOS_ENTITY_ID);
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void AppSonos::_handle_poll()
{
    if (_data.state != State::POLLING)
        return;

    if (millis() - _data.last_poll_ms < 3000)
        return;

    _data.last_poll_ms = millis();
    _refresh_state();
    _render();
}


void AppSonos::_render()
{
    if (_data.state == State::CONNECTING)
    {
        _gui.renderStatus("Connecting...", "");
    }
    else if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        int volume_percent = (int)(_data.volume * 100.0f + 0.5f);
        _gui.renderPlaying(_data.title, _data.artist, _data.is_playing, volume_percent);
    }
}


void AppSonos::onRunning()
{
    _handle_encoder();
    _handle_volume_debounce();
    _handle_touch();
    _handle_poll();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void AppSonos::onDestroy()
{
    _log("onDestroy");

    WIFI_CONNECT::disconnect();

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with
       (same fix already applied for AppTimer). */
    _data.hal->canvas->setTextSize(1);
}
