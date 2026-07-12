/**
 * @file app_rtc_test.h
 * @brief Controls the floor fan's power, oscillation, and speed through
 * Home Assistant's REST API. Touch: power/swing toggles. Encoder:
 * debounced speed. Encoder button: quit (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_rtc_test.h"
#include "fan_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace RTC_TEST
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                bool fan_on = false;
                int percentage = 50;
                bool oscillating = false;

                bool percentage_dirty = false;
                uint32_t last_percentage_change_ms = 0;
            };
        } // namespace RTC_TEST

        class RTC_Test : public APP_BASE
        {
        private:
            const char* _tag = "rtc";

            void _handle_encoder();
            void _handle_touch();
            void _handle_percentage_debounce();
            void _render();
            void _refresh_state();

        public:
            RTC_TEST::Data_t _data;
            GUI_RTC_Test _gui;

            GUI_Base* getGui() override { return &_gui; }

            void onSetup();
            void onCreate();
            void onRunning();
            void onDestroy();
        };

    } // namespace USER_APP
} // namespace MOONCAKE
