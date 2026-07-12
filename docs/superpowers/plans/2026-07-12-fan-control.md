# Fan Control (RTC TEST → FAN) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repurpose `app_rtc_test` (clock display) into a Home Assistant fan controller for `fan.dmaker_cn_740412216_p5c_s_2_fan` — power toggle, oscillation toggle, encoder-controlled speed.

**Architecture:** Same state machine and file layout as `app_set_brightness` (fish tank light): `CONNECTING`/`CONTROLLING`/`ERROR`, persistent WiFi (connect() is a no-op, no disconnect() in onDestroy), debounced encoder writes. `HA_CLIENT` gets 4 new fan functions. Class names (`RTC_Test`, `GUI_RTC_Test`) and launcher wiring stay unchanged — only behavior, config, and the launcher tag/icon change.

**Tech Stack:** ESP-IDF v5.1.3, esp_http_client + cJSON (existing `HA_CLIENT`), LovyanGFX (existing `GUI_Base`).

## Global Constraints

- WiFi is already connected at boot (`main.cpp`) — this app's `onCreate()` still calls `WIFI_CONNECT::connect()` (fast no-op) but `onDestroy()` must NOT call `WIFI_CONNECT::disconnect()` (breaks RFID_SERVICE).
- Encoder button press-and-release = unconditional quit (project-wide convention).
- Config secrets go in a gitignored `fan_config.h` + committed `fan_config.example.h` template, same WiFi/HA credentials as the other 3 HA apps.
- Speed steps in 10% increments (0-100), debounced 400ms before sending to HA (same pattern as brightness/volume).

---

### Task 1: Add fan functions to `HA_CLIENT`

**Files:**
- Modify: `main/apps/utilities/ha_client/ha_client.h`
- Modify: `main/apps/utilities/ha_client/ha_client.cpp`

**Interfaces:**
- Produces: `HA_CLIENT::FanState{ok, is_on, percentage, oscillating}`, `HA_CLIENT::get_fan_state(base_url, token, entity_id) -> FanState`, `HA_CLIENT::set_fan_power(base_url, token, entity_id, bool on) -> bool`, `HA_CLIENT::set_fan_percentage(base_url, token, entity_id, int percentage) -> bool`, `HA_CLIENT::set_fan_oscillating(base_url, token, entity_id, bool oscillating) -> bool`. Consumed by Task 3 (`app_rtc_test.cpp`).

- [ ] **Step 1: Add `FanState` struct and function declarations to `ha_client.h`**

Append to the end of the `HA_CLIENT` namespace in `main/apps/utilities/ha_client/ha_client.h` (right before the closing `}`):

```cpp
    struct FanState
    {
        bool ok = false;
        bool is_on = false;
        int percentage = 0;      // 0-100
        bool oscillating = false;
    };

    /**
     * @brief GET /api/states/{entity_id} for a fan entity
     */
    FanState get_fan_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/fan/turn_on or /turn_off (no extra fields)
     */
    bool set_fan_power(const char* base_url, const char* token,
                        const char* entity_id, bool on);

    /**
     * @brief POST /api/services/fan/set_percentage with
     * {"entity_id": entity_id, "percentage": percentage}
     */
    bool set_fan_percentage(const char* base_url, const char* token,
                             const char* entity_id, int percentage);

    /**
     * @brief POST /api/services/fan/oscillate with
     * {"entity_id": entity_id, "oscillating": true/false}
     */
    bool set_fan_oscillating(const char* base_url, const char* token,
                              const char* entity_id, bool oscillating);
```

- [ ] **Step 2: Implement the 4 functions in `ha_client.cpp`**

Append to the end of the `HA_CLIENT` namespace in `main/apps/utilities/ha_client/ha_client.cpp` (right before the closing `}`):

```cpp

    FanState get_fan_state(const char* base_url, const char* token, const char* entity_id)
    {
        FanState result;

        char url[256];
        snprintf(url, sizeof(url), "%s/api/states/%s", base_url, entity_id);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_handle_t client = _make_client(url, token, HTTP_METHOD_GET, &resp_buf);
        esp_err_t err = esp_http_client_perform(client);

        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "get_fan_state failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_fan_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.is_on = (strcmp(state->valuestring, "on") == 0);
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* percentage = cJSON_GetObjectItem(attributes, "percentage");
            if (cJSON_IsNumber(percentage))
            {
                result.percentage = percentage->valueint;
            }

            cJSON* oscillating = cJSON_GetObjectItem(attributes, "oscillating");
            if (cJSON_IsBool(oscillating))
            {
                result.oscillating = cJSON_IsTrue(oscillating);
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_fan_power(const char* base_url, const char* token,
                        const char* entity_id, bool on)
    {
        return call_service(base_url, token, "fan", on ? "turn_on" : "turn_off", entity_id);
    }


    bool set_fan_percentage(const char* base_url, const char* token,
                             const char* entity_id, int percentage)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"percentage\": %d}",
                 entity_id, percentage);

        return _post_json(base_url, token, "/api/services/fan/set_percentage", body);
    }


    bool set_fan_oscillating(const char* base_url, const char* token,
                              const char* entity_id, bool oscillating)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"oscillating\": %s}",
                 entity_id, oscillating ? "true" : "false");

        return _post_json(base_url, token, "/api/services/fan/oscillate", body);
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
Add fan domain support to HA_CLIENT

get_fan_state reads state/percentage/oscillating; set_fan_power,
set_fan_percentage, set_fan_oscillating call the matching fan.*
services. Not yet used - no caller until the fan control app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Config files

**Files:**
- Create: `main/apps/app_rtc_test/fan_config.example.h`
- Create: `main/apps/app_rtc_test/fan_config.h` (gitignored)
- Modify: `.gitignore`

**Interfaces:**
- Produces: `FAN_WIFI_SSID`, `FAN_WIFI_PASSWORD`, `FAN_HA_BASE_URL`, `FAN_HA_TOKEN`, `FAN_ENTITY_ID` macros. Consumed by Task 3.

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/utilities/rfid_service/rfid_service_config.h` entry:

```
main/apps/app_rtc_test/fan_config.h
```

- [ ] **Step 2: Write the committed example config**

Write `main/apps/app_rtc_test/fan_config.example.h`:

```cpp
/**
 * @file fan_config.example.h
 * @brief Copy this file to fan_config.h (gitignored) and fill in real
 * values. fan_config.h is never committed.
 */
#pragma once

#define FAN_WIFI_SSID       "YOUR_WIFI_SSID"
#define FAN_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define FAN_HA_BASE_URL     "http://YOUR_HA_HOST:8123"
#define FAN_HA_TOKEN        "YOUR_LONG_LIVED_ACCESS_TOKEN"
#define FAN_ENTITY_ID       "fan.your_entity"
```

- [ ] **Step 3: Write the real (gitignored) config**

Write `main/apps/app_rtc_test/fan_config.h` using the same WiFi/HA credentials already in `main/apps/app_sonos/sonos_config.h`, and the confirmed fan entity:

```cpp
/**
 * @file fan_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define FAN_WIFI_SSID       "<same SSID as sonos_config.h>"
#define FAN_WIFI_PASSWORD   "<same password as sonos_config.h>"
#define FAN_HA_BASE_URL     "<same base URL as sonos_config.h>"
#define FAN_HA_TOKEN        "<same token as sonos_config.h>"
#define FAN_ENTITY_ID       "fan.dmaker_cn_740412216_p5c_s_2_fan"
```

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add .gitignore main/apps/app_rtc_test/fan_config.example.h
git commit -m "$(cat <<'EOF'
Add fan_config template for the fan control app

Real fan_config.h is gitignored, not part of this commit.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`fan_config.h` itself is gitignored and won't show up in `git status` as trackable — nothing more to commit for it.)

---

### Task 3: Replace `app_rtc_test.h` / `.cpp` with the fan controller

**Files:**
- Modify: `main/apps/app_rtc_test/app_rtc_test.h`
- Modify: `main/apps/app_rtc_test/app_rtc_test.cpp`

**Interfaces:**
- Consumes: `HA_CLIENT::get_fan_state/set_fan_power/set_fan_percentage/set_fan_oscillating` (Task 1), `FAN_WIFI_SSID/FAN_WIFI_PASSWORD/FAN_HA_BASE_URL/FAN_HA_TOKEN/FAN_ENTITY_ID` (Task 2), `WIFI_CONNECT::connect` (existing).
- Produces: `GUI_RTC_Test::renderStatus(line1, line2)` and `GUI_RTC_Test::renderPage(bool fanOn, int percentage, bool oscillating)` calls — signature defined in Task 4, this task just calls it.

- [ ] **Step 1: Replace `main/apps/app_rtc_test/app_rtc_test.h`**

```cpp
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
```

- [ ] **Step 2: Replace `main/apps/app_rtc_test/app_rtc_test.cpp`**

```cpp
/**
 * @file app_rtc_test.cpp
 */
#include "app_rtc_test.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::RTC_TEST;


void RTC_Test::onSetup()
{
    setAppName("RTC_Test");
    setAllowBgRunning(false);

    RTC_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void RTC_Test::_refresh_state()
{
    HA_CLIENT::FanState fan = HA_CLIENT::get_fan_state(
        FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID);

    if (!fan.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.fan_on = fan.is_on;
    _data.oscillating = fan.oscillating;
    if (!_data.percentage_dirty)
    {
        _data.percentage = fan.percentage;
    }
}


void RTC_Test::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
    _render();

    bool connected = WIFI_CONNECT::connect(FAN_WIFI_SSID, FAN_WIFI_PASSWORD, 8000);
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


void RTC_Test::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    _data.percentage += direction * 10;
    if (_data.percentage < 0) _data.percentage = 0;
    if (_data.percentage > 100) _data.percentage = 100;

    _data.percentage_dirty = true;
    _data.last_percentage_change_ms = millis();

    _render();
}


void RTC_Test::_handle_percentage_debounce()
{
    if (!_data.percentage_dirty)
        return;

    if (millis() - _data.last_percentage_change_ms < 400)
        return;

    HA_CLIENT::set_fan_percentage(FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID, _data.percentage);
    _data.percentage_dirty = false;
}


void RTC_Test::_handle_touch()
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
            _data.fan_on = !_data.fan_on;
            HA_CLIENT::set_fan_power(FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID, _data.fan_on);
            _refresh_state();
            _render();
        }
        else if (x >= 122 && x <= 195)
        {
            /* SWING button */
            _data.oscillating = !_data.oscillating;
            HA_CLIENT::set_fan_oscillating(FAN_HA_BASE_URL, FAN_HA_TOKEN, FAN_ENTITY_ID, _data.oscillating);
            _refresh_state();
            _render();
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void RTC_Test::_render()
{
    if (_data.state == State::CONNECTING)
    {
        _gui.renderStatus("Connecting...", "");
    }
    else if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        _gui.renderPage(_data.fan_on, _data.percentage, _data.oscillating);
    }
}


void RTC_Test::onRunning()
{
    _handle_encoder();
    _handle_percentage_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void RTC_Test::onDestroy()
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

This will fail until Task 4 adds the new `GUI_RTC_Test::renderStatus`/`renderPage` signatures — build after Task 4 instead. Skip building at the end of this task; proceed directly to Task 4.

---

### Task 4: Replace `gui_rtc_test.h` / `.cpp` with the fan controller display

**Files:**
- Modify: `main/apps/app_rtc_test/gui/gui_rtc_test.h`
- Modify: `main/apps/app_rtc_test/gui/gui_rtc_test.cpp`

**Interfaces:**
- Consumes: nothing new (pure rendering, same `GUI_Base` helpers `_draw_top_icon()`/`_draw_quit_button()`/`_theme_color`/`_canvas` used by every other app's GUI).
- Produces: `GUI_RTC_Test::renderStatus(const std::string& line1, const std::string& line2)`, `GUI_RTC_Test::renderPage(bool fanOn, int percentage, bool oscillating)` — consumed by Task 3.

- [ ] **Step 1: Replace `main/apps/app_rtc_test/gui/gui_rtc_test.h`**

```cpp
/**
 * @file gui_rtc_test.h
 * @brief Renderer for the fan control app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_RTC_Test : public GUI_Base
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
         * @param fanOn current on/off state (affects the POWER button label)
         * @param percentage 0-100 fan speed
         * @param oscillating current swing state (affects the SWING button highlight)
         */
        void renderPage(bool fanOn, int percentage, bool oscillating);
};
```

- [ ] **Step 2: Replace `main/apps/app_rtc_test/gui/gui_rtc_test.cpp`**

```cpp
/**
 * @file gui_rtc_test.cpp
 */
#include "gui_rtc_test.h"
#include <cstdio>


void GUI_RTC_Test::init()
{
}


void GUI_RTC_Test::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_RTC_Test::renderPage(bool fanOn, int percentage, bool oscillating)
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
    _canvas->drawCenterString("SPEED", bubble.x, bubble.y - 48);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", percentage);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 30);

    _canvas->setTextSize(1);
    _canvas->drawCenterString("FAN", bubble.x, bubble.y + 34);

    /* POWER and SWING buttons, side by side below the bubble */
    int btn_y = 206;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* power_label = fanOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 85;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    /* SWING button: filled with the theme accent when oscillating is on,
       white otherwise - same on/off highlight convention used for the
       selected field in the Timer app */
    const char* swing_label = "SWING";
    int swing_text_w = _canvas->textWidth(swing_label);
    int swing_text_h = _canvas->fontHeight();
    int swing_btn_width = swing_text_w + 30;
    if (swing_btn_width < 60) swing_btn_width = 60;
    int swing_btn_x = 155;

    uint32_t swing_btn_color = oscillating ? TFT_WHITE : 0x666666U;
    uint32_t swing_text_color = oscillating ? _theme_color : TFT_WHITE;

    _canvas->fillSmoothRoundRect(swing_btn_x - swing_btn_width / 2, btn_y - btn_height / 2,
                                  swing_btn_width, btn_height, btn_height / 2, swing_btn_color);
    _canvas->setTextColor(swing_text_color);
    _canvas->drawCenterString(swing_label, swing_btn_x, btn_y - swing_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
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
git add main/apps/app_rtc_test/app_rtc_test.h main/apps/app_rtc_test/app_rtc_test.cpp main/apps/app_rtc_test/gui/gui_rtc_test.h main/apps/app_rtc_test/gui/gui_rtc_test.cpp
git commit -m "$(cat <<'EOF'
Turn RTC_Test into a Home Assistant fan controller

Touch POWER toggles on/off, touch SWING toggles oscillation, encoder
adjusts speed in 10% steps (debounced). Same CONNECTING/CONTROLLING/
ERROR state machine and persistent-WiFi convention as the other 3 HA
apps. Class names and launcher wiring unchanged.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Rename the launcher tag and swap the icon

**Files:**
- Modify: `main/apps/launcher/launcher_icons/launcher_icons.h`
- Modify: `main/apps/launcher/launcher_render_callback.hpp`

**Interfaces:** none new — `icon_fan.h` (already committed in the design-spec commit) defines `image_data_icon_fan`, a `const uint16_t[1764]` array, same shape as every other icon array.

- [ ] **Step 1: Include the new icon header**

In `main/apps/launcher/launcher_icons/launcher_icons.h`, add (alphabetically, after `icon_brigntness.h`):

```cpp
#include "icon_fan.h"
```

(Leave `#include "icon_rtc.h"` in place — no other icon still needs it, but removing unused-but-harmless includes isn't part of this task's scope.)

- [ ] **Step 2: Swap the icon array and rename the tag**

In `main/apps/launcher/launcher_render_callback.hpp`:

Change the `icon_tag_list` entry (currently `"RTC", "TIME"`, second row) to:
```cpp
    "FAN", "CTRL",
```

Change the `icon_pic_list` entry (currently `image_data_icon_rtc`, second row) to:
```cpp
    image_data_icon_fan,
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
git add main/apps/launcher/launcher_icons/launcher_icons.h main/apps/launcher/launcher_render_callback.hpp
git commit -m "$(cat <<'EOF'
Rename launcher tag RTC/TIME to FAN/CTRL, swap in the fan icon

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

- Launcher shows "FAN"/"CTRL" tag with the new fan icon in the RTC slot.
- Opening the app shows "Connecting..." briefly (or instantly, since WiFi is already up), then the control screen.
- Tapping POWER toggles the fan on/off in Home Assistant (verify in the HA app or by listening to the physical fan).
- Tapping SWING toggles oscillation; the button highlights (white bg) when swing is on.
- Rotating the encoder changes the displayed speed % in 10% steps; after ~400ms the physical fan's speed changes to match.
- Quitting (encoder button press-and-release) works from every state (CONNECTING/CONTROLLING/ERROR).
- Tapping the phone RFID card while this app is open still turns off the lights/fan (RFID_SERVICE runs independently — regression check from the previous feature).
