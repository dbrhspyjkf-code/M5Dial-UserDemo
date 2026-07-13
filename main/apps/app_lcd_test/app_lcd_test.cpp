/**
 * @file app_lcd_test.cpp
 */
#include "app_lcd_test.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::LCD_TEST;


void LCD_Test::onSetup()
{
    setAppName("LCD_Test");
    setAllowBgRunning(false);

    LCD_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void LCD_Test::_refresh_state()
{
    HA_CLIENT::SwitchState sw = HA_CLIENT::get_switch_state(
        TV_HA_BASE_URL, TV_HA_TOKEN, TV_SWITCH_ENTITY_ID);

    if (!sw.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    /* Inverted: switch ON means the TV is OFF (sound-bar-only mode) */
    _data.tv_on = !sw.is_on;

    HA_CLIENT::NumberState num = HA_CLIENT::get_number_state(
        TV_HA_BASE_URL, TV_HA_TOKEN, TV_VOLUME_ENTITY_ID);

    if (!num.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.volume_min = num.min;
    _data.volume_max = num.max;
    if (!_data.volume_dirty)
    {
        _data.volume = num.value;
    }
}


void LCD_Test::onCreate()
{
    _log("onCreate");

    /* Render the control screen immediately with default values instead
       of a blank frame or a "Connecting..." screen - WiFi/HA_CLIENT's
       connection are already up from boot, so the real fetch below is
       normally fast enough that this briefly-stale render is barely
       noticeable, then gets replaced by the real state. */
    _data.state = State::CONTROLLING;
    _render();

    bool connected = WIFI_CONNECT::connect(TV_WIFI_SSID, TV_WIFI_PASSWORD, 8000);
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


void LCD_Test::_handle_encoder()
{
    if (_data.state != State::CONTROLLING || _data.page != Page::VOLUME)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    /* A full sweep of the range takes ~20 detents, regardless of the
       entity's actual scale */
    float step = (_data.volume_max - _data.volume_min) / 20.0f;
    float direction = (_data.hal->encoder.getDirection() < 1) ? 1.0f : -1.0f;

    _data.volume += direction * step;
    if (_data.volume < _data.volume_min) _data.volume = _data.volume_min;
    if (_data.volume > _data.volume_max) _data.volume = _data.volume_max;

    _data.volume_dirty = true;
    _data.last_volume_change_ms = millis();

    _render();
}


void LCD_Test::_handle_volume_debounce()
{
    if (!_data.volume_dirty)
        return;

    if (millis() - _data.last_volume_change_ms < 400)
        return;

    HA_CLIENT::set_number_value(TV_HA_BASE_URL, TV_HA_TOKEN, TV_VOLUME_ENTITY_ID, _data.volume);
    _data.volume_dirty = false;
}


void LCD_Test::_handle_touch_volume_page(int x, int y)
{
    if (y < 182 || y > 206)
        return;

    if (x >= 30 && x <= 118)
    {
        /* MODE button - switch to NAV page */
        _data.page = Page::NAV;
        _render();
    }
    else if (x >= 122 && x <= 210)
    {
        /* POWER button */
        _data.tv_on = !_data.tv_on;

        /* Inverted: turning the TV on means turning the switch OFF */
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "switch",
                                 _data.tv_on ? "turn_off" : "turn_on", TV_SWITCH_ENTITY_ID);
        _refresh_state();
        _render();
    }
}


void LCD_Test::_handle_touch_nav_page(int x, int y)
{
    /* D-pad cross, centered around (120, 112) */
    if (x >= 101 && x <= 139 && y >= 58 && y <= 90)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_UP_ENTITY_ID);
    }
    else if (x >= 101 && x <= 139 && y >= 134 && y <= 166)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_DOWN_ENTITY_ID);
    }
    else if (x >= 41 && x <= 79 && y >= 96 && y <= 128)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_LEFT_ENTITY_ID);
    }
    else if (x >= 161 && x <= 199 && y >= 96 && y <= 128)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_RIGHT_ENTITY_ID);
    }
    else if (x >= 101 && x <= 139 && y >= 96 && y <= 128)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_OK_ENTITY_ID);
    }
    else if (x >= 16 && x <= 80 && y >= 178 && y <= 206)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_BACK_ENTITY_ID);
    }
    else if (x >= 88 && x <= 152 && y >= 178 && y <= 206)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_MENU_ENTITY_ID);
    }
    else if (x >= 160 && x <= 224 && y >= 178 && y <= 206)
    {
        /* VOL button - switch back to VOLUME page */
        _data.page = Page::VOLUME;
        _render();
    }
}


void LCD_Test::_handle_touch()
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
    else if (_data.state == State::CONTROLLING)
    {
        if (_data.page == Page::VOLUME)
        {
            _handle_touch_volume_page(x, y);
        }
        else
        {
            _handle_touch_nav_page(x, y);
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void LCD_Test::_render()
{
    if (_data.state == State::CONNECTING)
    {
        _gui.renderStatus("Connecting...", "");
    }
    else if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else if (_data.page == Page::NAV)
    {
        _gui.renderNavPage();
    }
    else
    {
        float range = _data.volume_max - _data.volume_min;
        int volume_pct = (range > 0.0f)
            ? (int)((_data.volume - _data.volume_min) / range * 100.0f + 0.5f)
            : 0;

        _gui.renderVolumePage(volume_pct, _data.tv_on);
    }
}


void LCD_Test::onRunning()
{
    _handle_encoder();
    _handle_volume_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void LCD_Test::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
