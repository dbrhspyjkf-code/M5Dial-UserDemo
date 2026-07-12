# AC Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repurpose `app_temp_demo` (`VideoShit`/`GUI_VideoShit`) into a Home Assistant climate controller for the master bedroom air conditioner `climate.lumi_cn_74788630_v3` — power toggle, HVAC mode cycling, encoder-controlled target temperature.

**Architecture:** Same state machine and file layout as `app_set_brightness` (fish tank light) and `app_rtc_test` (fan): `CONNECTING`/`CONTROLLING`/`ERROR`, persistent WiFi (connect() is a no-op, no disconnect() in onDestroy), immediate render with default values (no "Connecting..." flash), debounced encoder writes. `HA_CLIENT` gets 3 new climate functions. Class names (`VideoShit`, `GUI_VideoShit`) and launcher wiring stay unchanged — only behavior, config, and the launcher tag change (icon stays the same thermometer graphic).

**Tech Stack:** ESP-IDF v5.1.3, esp_http_client + cJSON (existing `HA_CLIENT`), LovyanGFX (existing `GUI_Base`).

## Global Constraints

- WiFi is already connected at boot (`main.cpp`) — `onCreate()` still calls `WIFI_CONNECT::connect()` (fast no-op) but `onDestroy()` must NOT call `WIFI_CONNECT::disconnect()`.
- Encoder button press-and-release = unconditional quit (project-wide convention).
- Render the control screen immediately with default/last-known values in `onCreate()` (no blank flash, no "Connecting..." screen) — same convention just established for the other 4 HA apps.
- Config secrets go in a gitignored `ac_config.h` + committed `ac_config.example.h` template, same WiFi/HA credentials as the other 4 HA apps.
- Temperature steps in 0.5°C increments, clamped to `[min_temp, max_temp]`, debounced 400ms before sending to HA (same pattern as brightness/volume/fan-speed).
- No fan-speed or swing control — out of scope.

---

### Task 1: Add climate functions to `HA_CLIENT`

**Files:**
- Modify: `main/apps/utilities/ha_client/ha_client.h`
- Modify: `main/apps/utilities/ha_client/ha_client.cpp`

**Interfaces:**
- Produces: `HA_CLIENT::ClimateState{ok, hvac_mode, target_temp, current_temp, hvac_modes, min_temp, max_temp}`, `HA_CLIENT::get_climate_state(base_url, token, entity_id) -> ClimateState`, `HA_CLIENT::set_climate_temperature(base_url, token, entity_id, float temperature) -> bool`, `HA_CLIENT::set_climate_hvac_mode(base_url, token, entity_id, const char* hvac_mode) -> bool`. Consumed by Task 3 (`app_temp_demo.cpp`).

- [ ] **Step 1: Add `ClimateState` struct and function declarations to `ha_client.h`**

Append to the end of the `HA_CLIENT` namespace in `main/apps/utilities/ha_client/ha_client.h` (right before the closing `}`):

```cpp
    struct ClimateState
    {
        bool ok = false;
        std::string hvac_mode;                 // e.g. "cool", "heat", "off", "fan_only", "dry", "auto"
        float target_temp = 0.0f;
        float current_temp = 0.0f;
        std::vector<std::string> hvac_modes;    // modes this entity actually supports
        float min_temp = 16.0f;
        float max_temp = 30.0f;
    };

    /**
     * @brief GET /api/states/{entity_id} for a climate entity. The
     * entity's top-level "state" IS the current hvac_mode itself (not
     * "on"/"off"), same shape as the number domain's value-as-state.
     */
    ClimateState get_climate_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/climate/set_temperature with
     * {"entity_id": entity_id, "temperature": temperature}
     */
    bool set_climate_temperature(const char* base_url, const char* token,
                                  const char* entity_id, float temperature);

    /**
     * @brief POST /api/services/climate/set_hvac_mode with
     * {"entity_id": entity_id, "hvac_mode": hvac_mode}
     */
    bool set_climate_hvac_mode(const char* base_url, const char* token,
                                const char* entity_id, const char* hvac_mode);
```

- [ ] **Step 2: Implement the 3 functions in `ha_client.cpp`**

Append to the end of the `HA_CLIENT` namespace in `main/apps/utilities/ha_client/ha_client.cpp` (right before the closing `}`):

```cpp

    ClimateState get_climate_state(const char* base_url, const char* token, const char* entity_id)
    {
        ClimateState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_climate_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.hvac_mode = state->valuestring;
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* target_temp = cJSON_GetObjectItem(attributes, "temperature");
            if (cJSON_IsNumber(target_temp))
            {
                result.target_temp = (float)target_temp->valuedouble;
            }

            cJSON* current_temp = cJSON_GetObjectItem(attributes, "current_temperature");
            if (cJSON_IsNumber(current_temp))
            {
                result.current_temp = (float)current_temp->valuedouble;
            }

            cJSON* min_temp = cJSON_GetObjectItem(attributes, "min_temp");
            if (cJSON_IsNumber(min_temp))
            {
                result.min_temp = (float)min_temp->valuedouble;
            }

            cJSON* max_temp = cJSON_GetObjectItem(attributes, "max_temp");
            if (cJSON_IsNumber(max_temp))
            {
                result.max_temp = (float)max_temp->valuedouble;
            }

            cJSON* hvac_modes = cJSON_GetObjectItem(attributes, "hvac_modes");
            if (cJSON_IsArray(hvac_modes))
            {
                int count = cJSON_GetArraySize(hvac_modes);
                for (int i = 0; i < count; i++)
                {
                    cJSON* item = cJSON_GetArrayItem(hvac_modes, i);
                    if (cJSON_IsString(item))
                    {
                        result.hvac_modes.push_back(item->valuestring);
                    }
                }
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_climate_temperature(const char* base_url, const char* token,
                                  const char* entity_id, float temperature)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"temperature\": %.1f}",
                 entity_id, temperature);

        return _post_json(base_url, token, "/api/services/climate/set_temperature", body);
    }


    bool set_climate_hvac_mode(const char* base_url, const char* token,
                                const char* entity_id, const char* hvac_mode)
    {
        char body[192];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"hvac_mode\": \"%s\"}",
                 entity_id, hvac_mode);

        return _post_json(base_url, token, "/api/services/climate/set_hvac_mode", body);
    }
```

- [ ] **Step 3: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.`

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/utilities/ha_client/ha_client.h main/apps/utilities/ha_client/ha_client.cpp
git commit -m "$(cat <<'EOF'
Add climate domain support to HA_CLIENT

get_climate_state reads hvac_mode/target_temp/current_temp/hvac_modes
/min_temp/max_temp; set_climate_temperature and set_climate_hvac_mode
call the matching climate.* services. Not yet used - no caller until
the AC control app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Config files

**Files:**
- Create: `main/apps/app_temp_demo/ac_config.example.h`
- Create: `main/apps/app_temp_demo/ac_config.h` (gitignored)
- Modify: `.gitignore`

**Interfaces:**
- Produces: `AC_WIFI_SSID`, `AC_WIFI_PASSWORD`, `AC_HA_BASE_URL`, `AC_HA_TOKEN`, `AC_ENTITY_ID` macros. Consumed by Task 3.

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/app_rtc_test/fan_config.h` entry:

```
main/apps/app_temp_demo/ac_config.h
```

- [ ] **Step 2: Write the committed example config**

Write `main/apps/app_temp_demo/ac_config.example.h`:

```cpp
/**
 * @file ac_config.example.h
 * @brief Copy this file to ac_config.h (gitignored) and fill in real
 * values. ac_config.h is never committed.
 */
#pragma once

#define AC_WIFI_SSID       "YOUR_WIFI_SSID"
#define AC_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define AC_HA_BASE_URL     "http://YOUR_HA_HOST:8123"
#define AC_HA_TOKEN        "YOUR_LONG_LIVED_ACCESS_TOKEN"
#define AC_ENTITY_ID       "climate.your_entity"
```

- [ ] **Step 3: Write the real (gitignored) config**

Write `main/apps/app_temp_demo/ac_config.h` using the same WiFi/HA credentials already in `main/apps/app_sonos/sonos_config.h`, and the confirmed AC entity:

```cpp
/**
 * @file ac_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define AC_WIFI_SSID       "<same SSID as sonos_config.h>"
#define AC_WIFI_PASSWORD   "<same password as sonos_config.h>"
#define AC_HA_BASE_URL     "<same base URL as sonos_config.h>"
#define AC_HA_TOKEN        "<same token as sonos_config.h>"
#define AC_ENTITY_ID       "climate.lumi_cn_74788630_v3"
```

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add .gitignore main/apps/app_temp_demo/ac_config.example.h
git commit -m "$(cat <<'EOF'
Add ac_config template for the AC control app

Real ac_config.h is gitignored, not part of this commit.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Replace `app_temp_demo.h` / `.cpp` with the AC controller

**Files:**
- Modify: `main/apps/app_temp_demo/app_temp_demo.h`
- Modify: `main/apps/app_temp_demo/app_temp_demo.cpp`

**Interfaces:**
- Consumes: `HA_CLIENT::get_climate_state/set_climate_temperature/set_climate_hvac_mode` (Task 1), `AC_WIFI_SSID/AC_WIFI_PASSWORD/AC_HA_BASE_URL/AC_HA_TOKEN/AC_ENTITY_ID` (Task 2), `WIFI_CONNECT::connect` (existing).
- Produces: calls into `GUI_VideoShit::renderStatus(line1, line2)` and `GUI_VideoShit::renderPage(bool acOn, float targetTemp, const std::string& modeName)` — signatures defined in Task 4.

- [ ] **Step 1: Replace `main/apps/app_temp_demo/app_temp_demo.h`**

```cpp
/**
 * @file app_temp_demo.h
 * @brief Controls the master bedroom air conditioner's power, HVAC
 * mode, and target temperature through Home Assistant's REST API.
 * Touch: power/mode toggles. Encoder: debounced target temperature.
 * Encoder button: quit (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_temp_demo.h"
#include "ac_config.h"
#include <vector>


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace VIDEO_SHIT
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                bool ac_on = false;
                std::string hvac_mode;          // current mode, "off" when powered off
                std::string last_active_mode;    // remembered mode to restore on power-on
                std::vector<std::string> hvac_modes;
                float target_temp = 26.0f;
                float min_temp = 16.0f;
                float max_temp = 30.0f;

                bool temp_dirty = false;
                uint32_t last_temp_change_ms = 0;
            };
        } // namespace VIDEO_SHIT

        class VideoShit : public APP_BASE
        {
            private:
                const char* _tag = "ac";

                void _handle_encoder();
                void _handle_touch();
                void _handle_temp_debounce();
                void _render();
                void _refresh_state();

            public:
                VideoShit() = default;
                ~VideoShit() = default;

                VIDEO_SHIT::Data_t _data;
                GUI_VideoShit _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
```

- [ ] **Step 2: Replace `main/apps/app_temp_demo/app_temp_demo.cpp`**

```cpp
/**
 * @file app_temp_demo.cpp
 */
#include "app_temp_demo.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::VIDEO_SHIT;


void VideoShit::onSetup()
{
    setAppName("VideoShit");
    setAllowBgRunning(false);

    VIDEO_SHIT::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void VideoShit::_refresh_state()
{
    HA_CLIENT::ClimateState climate = HA_CLIENT::get_climate_state(
        AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID);

    if (!climate.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.hvac_mode = climate.hvac_mode;
    _data.ac_on = (climate.hvac_mode != "off");
    if (_data.ac_on)
    {
        _data.last_active_mode = climate.hvac_mode;
    }
    _data.hvac_modes = climate.hvac_modes;
    _data.min_temp = climate.min_temp;
    _data.max_temp = climate.max_temp;
    if (!_data.temp_dirty)
    {
        _data.target_temp = climate.target_temp;
    }
}


void VideoShit::onCreate()
{
    _log("onCreate");

    /* Render the control screen immediately with default values instead
       of a blank frame or a "Connecting..." screen - WiFi/HA_CLIENT's
       connection are already up from boot, so the real fetch below is
       normally fast enough that this briefly-stale render is barely
       noticeable, then gets replaced by the real state. */
    _data.state = State::CONTROLLING;
    _render();

    bool connected = WIFI_CONNECT::connect(AC_WIFI_SSID, AC_WIFI_PASSWORD, 8000);
    if (!connected)
    {
        _data.state = State::ERROR;
        _data.error_message = "WiFi failed";
        _render();
        return;
    }

    _refresh_state();
    if (_data.state != State::ERROR)
    {
        _data.state = State::CONTROLLING;
    }
    _render();
}


void VideoShit::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    _data.target_temp += direction * 0.5f;
    if (_data.target_temp < _data.min_temp) _data.target_temp = _data.min_temp;
    if (_data.target_temp > _data.max_temp) _data.target_temp = _data.max_temp;

    _data.temp_dirty = true;
    _data.last_temp_change_ms = millis();

    _render();
}


void VideoShit::_handle_temp_debounce()
{
    if (!_data.temp_dirty)
        return;

    if (millis() - _data.last_temp_change_ms < 400)
        return;

    HA_CLIENT::set_climate_temperature(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID, _data.target_temp);
    _data.temp_dirty = false;
}


void VideoShit::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    _data.hal->tp.update();
    int x = _data.hal->tp.getTouchPointBuffer().x;
    int y = _data.hal->tp.getTouchPointBuffer().y;

    if (_data.state == State::ERROR)
    {
        _data.state = State::CONNECTING;
        _render();
        onCreate();
    }
    else if (_data.state == State::CONTROLLING && y >= 189 && y <= 223)
    {
        if (x >= 45 && x <= 118)
        {
            /* POWER button */
            if (_data.ac_on)
            {
                _data.last_active_mode = _data.hvac_mode;
                HA_CLIENT::set_climate_hvac_mode(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID, "off");
            }
            else
            {
                std::string mode_to_restore = _data.last_active_mode;
                if (mode_to_restore.empty())
                {
                    for (auto& m : _data.hvac_modes)
                    {
                        if (m != "off")
                        {
                            mode_to_restore = m;
                            break;
                        }
                    }
                }
                if (!mode_to_restore.empty())
                {
                    HA_CLIENT::set_climate_hvac_mode(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID, mode_to_restore.c_str());
                }
            }
            _refresh_state();
            _render();
        }
        else if (x >= 122 && x <= 195 && _data.ac_on)
        {
            /* MODE button - cycles to the next non-"off" mode, no-op while powered off */
            if (!_data.hvac_modes.empty())
            {
                int current_index = -1;
                for (size_t i = 0; i < _data.hvac_modes.size(); i++)
                {
                    if (_data.hvac_modes[i] == _data.hvac_mode)
                    {
                        current_index = (int)i;
                        break;
                    }
                }

                int next_index = current_index;
                for (size_t step = 0; step < _data.hvac_modes.size(); step++)
                {
                    next_index = (next_index + 1) % (int)_data.hvac_modes.size();
                    if (_data.hvac_modes[next_index] != "off")
                        break;
                }

                HA_CLIENT::set_climate_hvac_mode(AC_HA_BASE_URL, AC_HA_TOKEN, AC_ENTITY_ID,
                                                  _data.hvac_modes[next_index].c_str());
                _refresh_state();
                _render();
            }
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void VideoShit::_render()
{
    if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        _gui.renderPage(_data.ac_on, _data.target_temp, _data.hvac_mode);
    }
}


void VideoShit::onRunning()
{
    _handle_encoder();
    _handle_temp_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void VideoShit::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```

- [ ] **Step 3: Build to verify it compiles**

This will fail until Task 4 adds the new `GUI_VideoShit::renderStatus`/`renderPage` signatures and removes the old temp-demo-icon-array members - build after Task 4 instead. Skip building at the end of this task; proceed directly to Task 4.

---

### Task 4: Replace `gui_temp_demo.h` / `.cpp` with the AC controller display

**Files:**
- Modify: `main/apps/app_temp_demo/gui/gui_temp_demo.h`
- Modify: `main/apps/app_temp_demo/gui/gui_temp_demo.cpp`
- Delete: `main/apps/app_temp_demo/gui/temp_demo_icons/` (the old demo's numbered/button icon headers - no longer referenced by anything)

**Interfaces:**
- Consumes: nothing new (pure rendering, same `GUI_Base` helpers `_draw_top_icon()`/`_draw_quit_button()`/`_theme_color`/`_canvas` used by every other app's GUI).
- Produces: `GUI_VideoShit::renderStatus(const std::string& line1, const std::string& line2)`, `GUI_VideoShit::renderPage(bool acOn, float targetTemp, const std::string& modeName)` — consumed by Task 3.

- [ ] **Step 1: Replace `main/apps/app_temp_demo/gui/gui_temp_demo.h`**

```cpp
/**
 * @file gui_temp_demo.h
 * @brief Renderer for the AC control app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_VideoShit : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the connecting/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render the normal control screen
         *
         * @param acOn current power state (affects the POWER button label)
         * @param targetTemp target temperature in °C
         * @param modeName current HVAC mode name (e.g. "cool", "off")
         */
        void renderPage(bool acOn, float targetTemp, const std::string& modeName);
};
```

- [ ] **Step 2: Replace `main/apps/app_temp_demo/gui/gui_temp_demo.cpp`**

```cpp
/**
 * @file gui_temp_demo.cpp
 */
#include "gui_temp_demo.h"
#include <cstdio>


void GUI_VideoShit::init()
{
}


void GUI_VideoShit::renderStatus(const std::string& line1, const std::string& line2)
{
    _canvas->fillScreen(_theme_color);
    _draw_top_icon();

    BasicObeject_t bubble;
    bubble.x = 120;
    bubble.y = 120;
    bubble.width = 240;
    bubble.height = 140;
    _canvas->fillSmoothRoundRect(bubble.x - bubble.width / 2, bubble.y - bubble.height / 2,
                                  bubble.width, bubble.height, 36, TFT_WHITE);

    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), bubble.x, bubble.y - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), bubble.x, bubble.y + 30);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_VideoShit::renderPage(bool acOn, float targetTemp, const std::string& modeName)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    BasicObeject_t bubble;
    bubble.x = 120;
    bubble.y = 120;
    bubble.width = 240;
    bubble.height = 140;
    _canvas->fillSmoothRoundRect(bubble.x - bubble.width / 2, bubble.y - bubble.height / 2,
                                  bubble.width, bubble.height, 36, TFT_WHITE);

    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("TARGET", bubble.x, bubble.y - 48);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%.1f%cC", targetTemp, 176 /* degree symbol, ASCII 0xB0 */);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 30);

    _canvas->setTextSize(1);
    std::string mode_display = acOn ? modeName : "OFF";
    _canvas->drawCenterString(mode_display.c_str(), bubble.x, bubble.y + 34);

    /* POWER and MODE buttons, side by side below the bubble */
    int btn_y = 206;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* power_label = acOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 78;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 162;

    /* Dim the MODE button when powered off, since it's a no-op then */
    uint32_t mode_btn_color = acOn ? TFT_WHITE : 0x666666U;
    uint32_t mode_text_color = acOn ? _theme_color : TFT_WHITE;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, mode_btn_color);
    _canvas->setTextColor(mode_text_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
```

- [ ] **Step 3: Delete the old demo's icon assets**

```bash
rm -rf ~/Projects/M5Dial-UserDemo/main/apps/app_temp_demo/gui/temp_demo_icons
```

- [ ] **Step 4: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.`

- [ ] **Step 5: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_temp_demo/app_temp_demo.h main/apps/app_temp_demo/app_temp_demo.cpp main/apps/app_temp_demo/gui/gui_temp_demo.h main/apps/app_temp_demo/gui/gui_temp_demo.cpp
git add -u main/apps/app_temp_demo/gui/temp_demo_icons
git commit -m "$(cat <<'EOF'
Turn VideoShit (TEMP CTRL) into a Home Assistant AC controller

Touch POWER toggles power (remembers the last active HVAC mode to
restore on power-on), touch MODE cycles through the entity's actual
supported hvac_modes, encoder adjusts target temperature in 0.5C
steps (debounced). Same CONNECTING/CONTROLLING/ERROR state machine
and persistent-WiFi convention as the other 4 HA apps. Class names
and launcher wiring unchanged. Removed the old demo's now-unused
icon assets.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Rename the launcher tag

**Files:**
- Modify: `main/apps/launcher/launcher_render_callback.hpp`

**Interfaces:** none new — icon graphic (`image_data_icon_temp`) stays exactly as-is, only the tag text changes.

- [ ] **Step 1: Rename the tag**

In `main/apps/launcher/launcher_render_callback.hpp`, change the `icon_tag_list` entry (currently `"TEMP CTRL", "DEMO"`) to:
```cpp
    "AC", "CTRL",
```

- [ ] **Step 2: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.`

- [ ] **Step 3: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/launcher/launcher_render_callback.hpp
git commit -m "$(cat <<'EOF'
Rename launcher tag TEMP CTRL/DEMO to AC/CTRL

Icon graphic (thermometer) is unchanged - already fits an AC/
temperature-control app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Flash and manual verification

**Files:** none — device flash + on-device check.

- [ ] **Step 1: Check current device port**

```bash
ls /dev/cu.*
```

- [ ] **Step 2: Flash (confirm with the user before running — standing project convention)**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py -p <PORT> flash
```

- [ ] **Step 3: Manual verification checklist**

- Launcher shows "AC"/"CTRL" tag (thermometer icon unchanged).
- Opening the app shows the control screen immediately (no blank flash, no "Connecting...").
- Tapping POWER turns the AC off; tapping it again restores the mode it was in before (verify in the HA app or by listening to the physical unit).
- Tapping MODE while on cycles through the modes this specific unit supports (whatever HA reports for `hvac_modes` minus "off") and is a visible no-op (dimmed button, no HA call) while powered off.
- Rotating the encoder changes the displayed target temperature in 0.5C steps, clamped to the entity's actual min/max; after ~400ms the physical AC's target temperature changes to match.
- Quitting (encoder button press-and-release) works from every state (CONTROLLING/ERROR).
- Tapping the phone RFID card while this app is open still turns off the lights/fan (RFID_SERVICE runs independently - regression check).
- Leaving the device idle for 5 minutes while this app is open still turns the backlight off, and touching wakes it without also triggering the POWER/MODE button underneath (IDLE_SCREEN regression check).
