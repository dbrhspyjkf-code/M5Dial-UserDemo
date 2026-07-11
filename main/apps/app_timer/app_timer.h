/**
 * @file app_timer.h
 * @brief MM:SS countdown timer. Touch selects/starts/pauses/resets;
 * encoder rotate adjusts the selected field; encoder button quits
 * (project-wide convention, same as every other app here).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_timer.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace TIMER
        {
            enum class Field { MINUTES, SECONDS };
            enum class State { EDIT, RUNNING, PAUSED, DONE };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::EDIT;
                Field selected_field = Field::SECONDS;

                /* Last-edited duration, restored when RESET from DONE */
                int set_minutes = 5;
                int set_seconds = 0;

                /* Live countdown value while RUNNING/PAUSED/DONE */
                int remaining_seconds = 300;

                uint32_t last_tick_ms = 0;
                uint32_t last_blink_ms = 0;
                bool blink_on = true;
            };
        }

        class AppTimer : public APP_BASE
        {
            private:
                const char* _tag = "AppTimer";

                void _handle_encoder();
                void _handle_touch();
                void _handle_tick();
                void _render();
                std::string _pill_label();

            public:
                TIMER::Data_t _data;
                GUI_Timer _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
