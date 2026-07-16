/**
 * @file gui_set_brightness.h
 * @brief Renderer for the household lights control app. Pure display —
 * takes primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_SetBrightness : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the connecting/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render a plain on/off switch page
         *
         * @param name switch name, e.g. "筒灯" (may be Chinese)
         * @param isOn current on/off state
         * @param index 0-based position in the light list
         * @param count total number of lights (pages) in the list
         */
        void renderSwitch(const std::string& name, bool isOn, int index, int count);

        /**
         * @brief Render the master bedroom page - two independent,
         * stateless toggle buttons (大灯/小灯), no on/off state to show
         *
         * @param index 0-based position in the light list
         * @param count total number of lights (pages) in the list
         */
        void renderMasterBedroom(int index, int count);

        /**
         * @brief Render the fish tank light's brightness/effect page
         *
         * @param brightnessMode true = show/adjust brightness, false = show/adjust effect
         * @param brightnessPct 0-100 (shown when brightnessMode is true)
         * @param effectName current effect name, or "(no effects)" (shown when brightnessMode is false)
         * @param lightOn current on/off state (affects the ON/OFF button label)
         * @param index 0-based position in the light list
         * @param count total number of lights (pages) in the list
         */
        void renderFishtank(bool brightnessMode, int brightnessPct, const std::string& effectName,
                             bool lightOn, int index, int count);
};
