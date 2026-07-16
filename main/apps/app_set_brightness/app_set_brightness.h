/**
 * @file app_set_brightness.h
 * @brief Controls all the household lights through Home Assistant's
 * REST API. Encoder always browses between lights (same convention as
 * STOCK). Touch toggles whichever light is currently shown - a simple
 * on/off button for the plain switches, two independent toggle buttons
 * for the master bedroom's two-gang switch, or the fish tank light's
 * own brightness/effect controls (unchanged from before this app grew
 * to cover the other lights). Encoder button: quit (project-wide
 * convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_set_brightness.h"
#include "fishtank_config.h"
#include <vector>
#include <string>


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace SET_BRIGHTNESS
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };
            enum class ControlMode { BRIGHTNESS, EFFECT };

            struct SwitchItem
            {
                std::string name;
                std::string entity_id;
                bool is_on = false;
            };

            /* Page layout: 0..SWITCH_COUNT-1 = plain on/off switches,
               PAGE_MASTER_BEDROOM = two stateless toggle buttons,
               PAGE_FISHTANK = brightness/effect control (last, so the
               fish tank keeps its "last item" spot as before). */
            static const int SWITCH_COUNT = 7;
            static const int PAGE_MASTER_BEDROOM = SWITCH_COUNT;
            static const int PAGE_FISHTANK = SWITCH_COUNT + 1;
            static const int PAGE_COUNT = SWITCH_COUNT + 2;

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                int current_page = 0;
                std::vector<SwitchItem> switches;

                /* Fish tank light (PAGE_FISHTANK) - fields unchanged
                   from the single-purpose fish tank app this grew from */
                int brightness_pct = 50;
                bool light_on = true;
                bool brightness_dirty = false;
                uint32_t last_brightness_change_ms = 0;
                ControlMode control_mode = ControlMode::BRIGHTNESS;
                std::vector<std::string> effect_list;
                int effect_index = 0;
                bool effect_dirty = false;
                uint32_t last_effect_change_ms = 0;
            };
        }

        class Set_Brightness : public APP_BASE
        {
            private:
                const char* _tag = "brightness";

                void _handle_encoder();
                void _handle_touch();
                void _handle_brightness_debounce();
                void _handle_effect_debounce();
                void _render();
                void _fetch_all();
                void _refresh_fishtank_state();

            public:
                SET_BRIGHTNESS::Data_t _data;
                GUI_SetBrightness _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
