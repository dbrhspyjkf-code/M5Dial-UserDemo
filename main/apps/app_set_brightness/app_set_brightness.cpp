/**
 * @file app_set_brightness.cpp
 */
#include "app_set_brightness.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::SET_BRIGHTNESS;


void Set_Brightness::onSetup()
{
    setAppName("Set_Brightness");
    setAllowBgRunning(false);

    SET_BRIGHTNESS::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void Set_Brightness::_refresh_state()
{
    HA_CLIENT::LightState light = HA_CLIENT::get_light_state(
        FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN, FISHTANK_ENTITY_ID);

    if (!light.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.light_on = light.is_on;
    if (!_data.brightness_dirty)
    {
        _data.brightness_pct = light.brightness_pct;
    }

    _data.effect_list = light.effect_list;
    if (!_data.effect_list.empty())
    {
        for (size_t i = 0; i < _data.effect_list.size(); i++)
        {
            if (_data.effect_list[i] == light.effect)
            {
                _data.effect_index = (int)i;
                break;
            }
        }
    }
}


void Set_Brightness::onCreate()
{
    _log("onCreate");

    /* Render the control screen immediately with default values instead
       of a blank frame or a "Connecting..." screen - WiFi/HA_CLIENT's
       connection are already up from boot, so the real fetch below is
       normally fast enough that this briefly-stale render is barely
       noticeable, then gets replaced by the real state. */
    _data.state = State::CONTROLLING;
    _render();

    bool connected = WIFI_CONNECT::connect(FISHTANK_WIFI_SSID, FISHTANK_WIFI_PASSWORD, 8000);
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
        _data.state = State::CONTROLLING;
    }
    _render();
}


void Set_Brightness::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    if (_data.control_mode == ControlMode::BRIGHTNESS)
    {
        _data.brightness_pct += direction * 5;
        if (_data.brightness_pct < 0) _data.brightness_pct = 0;
        if (_data.brightness_pct > 100) _data.brightness_pct = 100;

        _data.brightness_dirty = true;
        _data.last_brightness_change_ms = millis();
    }
    else if (!_data.effect_list.empty())
    {
        int count = (int)_data.effect_list.size();
        _data.effect_index = ((_data.effect_index + direction) % count + count) % count;

        _data.effect_dirty = true;
        _data.last_effect_change_ms = millis();
    }

    _render();
}


void Set_Brightness::_handle_brightness_debounce()
{
    if (!_data.brightness_dirty)
        return;

    if (millis() - _data.last_brightness_change_ms < 400)
        return;

    HA_CLIENT::set_light_brightness(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN,
                                     FISHTANK_ENTITY_ID, _data.brightness_pct);
    _data.brightness_dirty = false;
}


void Set_Brightness::_handle_effect_debounce()
{
    if (!_data.effect_dirty)
        return;

    if (millis() - _data.last_effect_change_ms < 400)
        return;

    HA_CLIENT::set_light_effect(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN, FISHTANK_ENTITY_ID,
                                 _data.effect_list[_data.effect_index].c_str());
    _data.effect_dirty = false;
}


void Set_Brightness::_handle_touch()
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
        onCreate();
    }
    else if (_data.state == State::CONTROLLING && y >= 184 && y <= 212)
    {
        if (x >= 30 && x <= 118)
        {
            /* MODE button: only switch to EFFECT if there's something to select */
            if (_data.control_mode == ControlMode::BRIGHTNESS && !_data.effect_list.empty())
            {
                _data.control_mode = ControlMode::EFFECT;
            }
            else
            {
                _data.control_mode = ControlMode::BRIGHTNESS;
            }
            _render();
        }
        else if (x >= 122 && x <= 210)
        {
            _data.light_on = !_data.light_on;
            HA_CLIENT::set_light_power(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN,
                                        FISHTANK_ENTITY_ID, _data.light_on);
            _refresh_state();
            _render();
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void Set_Brightness::_render()
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
        std::string effect_name = _data.effect_list.empty()
            ? "(no effects)"
            : _data.effect_list[_data.effect_index];

        _gui.renderPage(_data.control_mode == ControlMode::BRIGHTNESS,
                         _data.brightness_pct, effect_name, _data.light_on);
    }
}


void Set_Brightness::onRunning()
{
    _handle_encoder();
    _handle_brightness_debounce();
    _handle_effect_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void Set_Brightness::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
