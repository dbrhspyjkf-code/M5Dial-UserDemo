/**
 * @file app_rtc_test.cpp
 */
#include "app_rtc_test.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::RTC_TEST;


void RTC_Test::onSetup()
{
    setAppName("RTC_Test");
    setAllowBgRunning(false);

    RTC_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void RTC_Test::_refresh_state()
{
    HA_CLIENT::FanState fan = HA_CLIENT::get_fan_state(
        FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID);

    if (!fan.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.fan_on = fan.is_on;
    _data.oscillating = fan.oscillating;
    if (!_data.percentage_dirty)
    {
        _data.percentage = fan.percentage;
    }
}


void RTC_Test::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
    _render();

    bool connected = WIFI_CONNECT::connect(FAN_WIFI_SSID, FAN_WIFI_PASSWORD, 8000);
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


void RTC_Test::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    _data.percentage += direction * 10;
    if (_data.percentage < 0) _data.percentage = 0;
    if (_data.percentage > 100) _data.percentage = 100;

    _data.percentage_dirty = true;
    _data.last_percentage_change_ms = millis();

    _render();
}


void RTC_Test::_handle_percentage_debounce()
{
    if (!_data.percentage_dirty)
        return;

    if (millis() - _data.last_percentage_change_ms < 400)
        return;

    HA_CLIENT::set_fan_percentage(FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID, _data.percentage);
    _data.percentage_dirty = false;
}


void RTC_Test::_handle_touch()
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
    else if (_data.state == State::CONTROLLING && y >= 189 && y <= 223)
    {
        if (x >= 45 && x <= 118)
        {
            /* POWER button */
            _data.fan_on = !_data.fan_on;
            HA_CLIENT::set_fan_power(FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID, _data.fan_on);
            _refresh_state();
            _render();
        }
        else if (x >= 122 && x <= 195)
        {
            /* SWING button */
            _data.oscillating = !_data.oscillating;
            HA_CLIENT::set_fan_oscillating(FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID, _data.oscillating);
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


void RTC_Test::_render()
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
        _gui.renderPage(_data.fan_on, _data.percentage, _data.oscillating);
    }
}


void RTC_Test::onRunning()
{
    _handle_encoder();
    _handle_percentage_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void RTC_Test::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
