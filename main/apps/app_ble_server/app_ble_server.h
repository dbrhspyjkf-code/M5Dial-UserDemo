/**
 * @file app_ble_server.h
 * @brief Read-only viewer for unread email, fetched once per app open
 * from the user's email-status-server (part of hermes-mcp-xiaozhi).
 * Encoder rotates through folders that have unread mail (wraps
 * around). No writes to any server - this app is display-only.
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_ble_server.h"
#include "email_config.h"
#include "../utilities/email_client/email_client.h"
#include <vector>


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace BLE_SERVER
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                std::vector<EMAIL_CLIENT::FolderInfo> folders;
                int current_index = 0;
            };
        }

        class BLE_Server : public APP_BASE
        {
            private:
                const char* _tag = "email";

                void _handle_encoder();
                void _handle_touch();
                void _render();
                void _fetch();

            public:
                BLE_SERVER::Data_t _data;
                GUI_BLE_Server _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
