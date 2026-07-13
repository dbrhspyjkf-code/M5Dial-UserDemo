/**
 * @file app_wifi_scan.cpp
 */
#include "app_wifi_scan.h"
#include "../common_define.h"
#include "../utilities/deepseek_client/deepseek_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::WIFI_SCAN;


void WiFi_Scan::onSetup()
{
    setAppName("WiFi_Scan");
    setAllowBgRunning(false);

    WIFI_SCAN::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void WiFi_Scan::_fetch()
{
    _data.balance = DEEPSEEK_CLIENT::get_balance(DEEPSEEK_API_BASE_URL);

    if (!_data.balance.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "Unreachable";
        return;
    }

    _data.state = State::CONTROLLING;
}


void WiFi_Scan::onCreate()
{
    _log("onCreate");

    _data.state = State::CONNECTING;
    _render();

    _fetch();
    _render();
}


void WiFi_Scan::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    if (_data.state == State::ERROR)
    {
        _data.state = State::CONNECTING;
        _render();
        _fetch();
        _render();
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void WiFi_Scan::_render()
{
    if (_data.state == State::CONNECTING)
    {
        _gui.renderStatus("Loading...", "");
    }
    else if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        _gui.renderPage(_data.balance.total_balance, _data.balance.currency,
                         _data.balance.granted_balance, _data.balance.topped_up_balance);
    }
}


void WiFi_Scan::onRunning()
{
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void WiFi_Scan::onDestroy()
{
    _log("onDestroy");

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
