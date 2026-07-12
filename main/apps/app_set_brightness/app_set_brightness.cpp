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
}


void Set_Brightness::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
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

    int step = (_data.hal->encoder.getDirection() < 1) ? 5 : -5;
    _data.brightness_pct += step;
    if (_data.brightness_pct < 0) _data.brightness_pct = 0;
    if (_data.brightness_pct > 100) _data.brightness_pct = 100;

    _data.brightness_dirty = true;
    _data.last_brightness_change_ms = millis();

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
        WIFI_CONNECT::disconnect();
        onCreate();
    }
    else if (_data.state == State::CONTROLLING && x >= 85 && x <= 155 && y >= 185 && y <= 219)
    {
        _data.light_on = !_data.light_on;
        HA_CLIENT::set_light_power(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN,
                                    FISHTANK_ENTITY_ID, _data.light_on);
        _refresh_state();
        _render();
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
        _gui.renderPage(_data.brightness_pct, _data.light_on);
    }
}


void Set_Brightness::onRunning()
{
    _handle_encoder();
    _handle_brightness_debounce();
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

    WIFI_CONNECT::disconnect();

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
