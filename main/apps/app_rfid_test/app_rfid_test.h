/**
 * @file app_rfid_test.h
 * @brief Thin viewer of RFID_SERVICE's scan state - does NOT own an
 * RC522 handle itself (RFID_SERVICE owns the one shared handle, created
 * once at boot in main.cpp, independent of this app's lifecycle).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_rfid_test.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace APP_RFID_TEST
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                uint64_t displayed_sn = 0;
            };
        }

        class RFID_Test : public APP_BASE
        {
            private:
                const char* _tag = "rfid";

            public:
                APP_RFID_TEST::Data_t _data;
                GUI_RFID_Test _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
