/**
 * @file app_rfid_test.cpp
 */
#include "app_rfid_test.h"
#include "../common_define.h"
#include "../utilities/rfid_service/rfid_service.h"


using namespace MOONCAKE::USER_APP;


void RFID_Test::onSetup()
{
    setAppName("RFID_Test");
    setAllowBgRunning(false);

    APP_RFID_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void RFID_Test::onCreate()
{
    _log("onCreate");

    _data.displayed_sn = RFID_SERVICE::last_scanned_sn();
    if (_data.displayed_sn != 0)
    {
        _gui.renderPage(_data.displayed_sn);
    }
}


void RFID_Test::onRunning()
{
    uint64_t current = RFID_SERVICE::last_scanned_sn();
    if (current != 0 && current != _data.displayed_sn)
    {
        _data.displayed_sn = current;
        _gui.renderPage(_data.displayed_sn);
    }

    /* If button pressed */
    if (!_data.hal->encoder.btn.read())
    {
        /* Hold until button release */
        while (!_data.hal->encoder.btn.read())
            delay(5);

        /* Bye */
        destroyApp();
    }
}


void RFID_Test::onDestroy()
{
    _log("onDestroy");
}
