/**
 * @file app_ble_server.cpp
 */
#include "app_ble_server.h"
#include "../common_define.h"
#include "../utilities/email_client/email_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::BLE_SERVER;


void BLE_Server::onSetup()
{
    setAppName("BLE_Server");
    setAllowBgRunning(false);

    BLE_SERVER::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void BLE_Server::_fetch()
{
    _data.folders = EMAIL_CLIENT::get_unread(EMAIL_API_BASE_URL);

    if (_data.folders.empty())
    {
        _data.state = State::ERROR;
        _data.error_message = "No unread mail";
        return;
    }

    if (_data.current_index >= (int)_data.folders.size())
    {
        _data.current_index = 0;
    }
    _data.state = State::CONTROLLING;
}


void BLE_Server::onCreate()
{
    _log("onCreate");

    _data.state = State::CONNECTING;
    _render();

    _fetch();
    _render();
}


void BLE_Server::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;
    int count = (int)_data.folders.size();

    _data.current_index = (_data.current_index + direction + count) % count;

    _render();
}


void BLE_Server::_handle_touch()
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


void BLE_Server::_render()
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
        const EMAIL_CLIENT::FolderInfo& folder = _data.folders[_data.current_index];
        _gui.renderPage(folder.name, folder.unread, folder.latest_from, folder.latest_subject,
                         _data.current_index, (int)_data.folders.size());
    }
}


void BLE_Server::onRunning()
{
    _handle_encoder();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void BLE_Server::onDestroy()
{
    _log("onDestroy");

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
