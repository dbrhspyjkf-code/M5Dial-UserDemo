/**
 * @file app_temp_demo.cpp
 */
#include "app_temp_demo.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::VIDEO_SHIT;


void VideoShit::onSetup()
{
    setAppName("VideoShit");
    setAllowBgRunning(false);

    VIDEO_SHIT::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void VideoShit::_refresh_state()
{
    HA_CLIENT::ClimateState climate = HA_CLIENT::get_climate_state(
        AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID);

    if (!climate.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.hvac_mode = climate.hvac_mode;
    _data.ac_on = (climate.hvac_mode != "off");
    if (_data.ac_on)
    {
        _data.last_active_mode = climate.hvac_mode;
    }
    _data.hvac_modes = climate.hvac_modes;
    _data.min_temp = climate.min_temp;
    _data.max_temp = climate.max_temp;
    if (!_data.temp_dirty)
    {
        _data.target_temp = climate.target_temp;
    }
}


void VideoShit::onCreate()
{
    _log("onCreate");

    /* Render the control screen immediately with default values instead
       of a blank frame or a "Connecting..." screen - WiFi/HA_CLIENT's
       connection are already up from boot, so the real fetch below is
       normally fast enough that this briefly-stale render is barely
       noticeable, then gets replaced by the real state. */
    _data.state = State::CONTROLLING;
    _render();

    bool connected = WIFI_CONNECT::connect(AC_WIFI_SSID, AC_WIFI_PASSWORD, 8000);
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


void VideoShit::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    _data.target_temp += direction * 1.0f;
    if (_data.target_temp < _data.min_temp) _data.target_temp = _data.min_temp;
    if (_data.target_temp > _data.max_temp) _data.target_temp = _data.max_temp;

    _data.temp_dirty = true;
    _data.last_temp_change_ms = millis();

    _render();
}


void VideoShit::_handle_temp_debounce()
{
    if (!_data.temp_dirty)
        return;

    if (millis() - _data.last_temp_change_ms < 400)
        return;

    HA_CLIENT::set_climate_temperature(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID, _data.target_temp);
    _data.temp_dirty = false;
}


void VideoShit::_handle_touch()
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
            if (_data.ac_on)
            {
                _data.last_active_mode = _data.hvac_mode;
                HA_CLIENT::set_climate_hvac_mode(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID, "off");
            }
            else
            {
                std::string mode_to_restore = _data.last_active_mode;
                if (mode_to_restore.empty())
                {
                    for (auto& m : _data.hvac_modes)
                    {
                        if (m != "off")
                        {
                            mode_to_restore = m;
                            break;
                        }
                    }
                }
                if (!mode_to_restore.empty())
                {
                    HA_CLIENT::set_climate_hvac_mode(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID, mode_to_restore.c_str());
                }
            }
            _refresh_state();
            _render();
        }
        else if (x >= 122 && x <= 195 && _data.ac_on)
        {
            /* MODE button - cycles to the next non-"off" mode, no-op while powered off */
            if (!_data.hvac_modes.empty())
            {
                int current_index = -1;
                for (size_t i = 0; i < _data.hvac_modes.size(); i++)
                {
                    if (_data.hvac_modes[i] == _data.hvac_mode)
                    {
                        current_index = (int)i;
                        break;
                    }
                }

                int next_index = current_index;
                for (size_t step = 0; step < _data.hvac_modes.size(); step++)
                {
                    next_index = (next_index + 1) % (int)_data.hvac_modes.size();
                    if (_data.hvac_modes[next_index] != "off")
                        break;
                }

                HA_CLIENT::set_climate_hvac_mode(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID,
                                                  _data.hvac_modes[next_index].c_str());
                _refresh_state();
                _render();
            }
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void VideoShit::_render()
{
    if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        _gui.renderPage(_data.ac_on, _data.target_temp, _data.hvac_mode);
    }
}


void VideoShit::onRunning()
{
    _handle_encoder();
    _handle_temp_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void VideoShit::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
