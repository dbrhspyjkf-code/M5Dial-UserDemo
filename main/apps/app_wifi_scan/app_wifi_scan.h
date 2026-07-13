/**
 * @file app_wifi_scan.h
 * @brief Read-only viewer for the DeepSeek API account balance,
 * fetched once per app open through hermes-mcp-xiaozhi. No writes to
 * any server - this app is display-only. No encoder interaction
 * (there's nothing to browse, just one account's balance) beyond the
 * project-wide quit gesture.
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_wifi_scan.h"
#include "deepseek_config.h"
#include "../utilities/deepseek_client/deepseek_client.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace WIFI_SCAN
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                DEEPSEEK_CLIENT::BalanceInfo balance;
            };
        }

        class WiFi_Scan : public APP_BASE
        {
            private:
                const char* _tag = "deepseek";

                void _handle_touch();
                void _render();
                void _fetch();

            public:
                WIFI_SCAN::Data_t _data;
                GUI_WiFi_Scan _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
