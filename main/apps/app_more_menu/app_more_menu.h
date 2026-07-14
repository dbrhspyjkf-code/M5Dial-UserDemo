/**
 * @file app_more_menu.h
 * @brief Read-only viewer for the user's stock watchlist, fetched
 * once per app open from the hermes-mcp-xiaozhi stock API. Encoder
 * rotates through the list (wraps around). Tapping the screen toggles
 * to that stock's analysis summary (already included in the fetched
 * data, no extra request) and tapping again returns to the list.
 * No writes to any server - this app is display-only.
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_more_menu.h"
#include "stock_config.h"
#include "../utilities/stock_client/stock_client.h"
#include <vector>


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace MORE_MENU
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                std::vector<STOCK_CLIENT::StockItem> stocks;
                int current_index = 0;
                bool showing_analysis = false;
            };
        } // namespace MORE_MENU

        class MoreMenu : public APP_BASE
        {
            private:
                const char* _tag = "stock";

                void _handle_encoder();
                void _handle_touch();
                void _render();
                void _fetch();

            public:
                MORE_MENU::Data_t _data;
                GUI_MoreMenu _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
