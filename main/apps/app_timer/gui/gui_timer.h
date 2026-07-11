/**
 * @file gui_timer.h
 * @brief Renderer for the Timer app. Pure display — takes primitive
 * values, draws them. No state lives here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_Timer : public GUI_Base
{
    private:

    public:
        void init() override;

        /**
         * @brief Render the timer page
         *
         * @param displayMinutes minutes value to show
         * @param displaySeconds seconds value to show
         * @param selectedField  0 = minutes highlighted, 1 = seconds highlighted, -1 = no highlight
         * @param pillLabel      text for the bottom pill button (START/PAUSE/RESUME/RESET)
         * @param showDigits     false to blank the digits (used for the DONE-state blink)
         */
        void renderPage(int displayMinutes, int displaySeconds, int selectedField,
                         const std::string& pillLabel, bool showDigits);
};
