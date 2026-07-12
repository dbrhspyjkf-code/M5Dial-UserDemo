# TV Control via Home Assistant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repurpose `app_lcd_test` (launcher slot 0, "LCD"/"TEST") from an LCD color-test page cycler into TV power+volume control via Home Assistant — `switch.xiaomi_esprh1_0bc4_is_on` (inverted: ON = TV off) and `number.xiaomi_esprh1_0bc4_volume`.

**Architecture:** Two new `ha_client` functions/structs for the `switch` and `number` HA domains, reusing the existing `wifi_connect_wrap`/`ha_client`/`_make_client`/`_post_json` infrastructure built for the Sonos and fish-tank-light apps. `app_lcd_test`'s internals are fully replaced with a CONNECTING/CONTROLLING/ERROR state machine (same shape as the other two HA-backed apps), single-value (volume only, no mode switch needed).

**Tech Stack:** ESP-IDF v5.1.3, existing `esp_http_client`/`cJSON` via `ha_client`, C++.

## Global Constraints

- No unit test harness — "test" per task means `idf.py build` succeeds. Full behavior is verified once, at the end, by flashing and manually exercising every control against the real TV.
- Encoder button press-and-release means "quit the app", unconditionally, in every state.
- Config is compile-time, gitignored, never committed — same pattern as `sonos_config.h`/`fishtank_config.h`.
- Build environment: `source ~/esp-idf-v5.1.3/export.sh` before any `idf.py` command. Flash port: check `ls /dev/cu.*` each time — confirmed `/dev/cu.usbmodem112401` as of the last flash, but it changes across sessions.
- **Power inversion is deliberate and must not leak into the UI**: `switch.xiaomi_esprh1_0bc4_is_on` = ON means the TV is OFF. The app tracks/displays `tv_on = !switch.is_on` and only ever shows "ON"/"OFF" meaning the TV's actual power state — never mentions "sound bar mode" anywhere in the UI.
- WiFi/HA credentials are the same ones already used for Sonos/fish-tank-light (same network, same HA instance) — reuse those exact values when creating `tv_config.h`, don't ask the user again.

---

### Task 1: `ha_client` switch and number domain support

**Files:**
- Modify: `main/apps/utilities/ha_client/ha_client.h`
- Modify: `main/apps/utilities/ha_client/ha_client.cpp`

**Interfaces:**
- Consumes: existing `_make_client()`, `_post_json()` private helpers, existing `call_service()` public function.
- Produces: `HA_CLIENT::SwitchState`/`get_switch_state()` and `HA_CLIENT::NumberState`/`get_number_state()`/`set_number_value()`, consumed by Task 3.

- [ ] **Step 1: Add declarations to `ha_client.h`**

Add this block after the existing `set_light_effect` declaration (before the closing `}` of the namespace):

```cpp
    struct SwitchState
    {
        bool ok = false;
        bool is_on = false;
    };

    /**
     * @brief GET /api/states/{entity_id} for a switch entity
     */
    SwitchState get_switch_state(const char* base_url, const char* token, const char* entity_id);

    struct NumberState
    {
        bool ok = false;
        float value = 0.0f;
        float min = 0.0f;
        float max = 100.0f;
    };

    /**
     * @brief GET /api/states/{entity_id} for a number entity. Unlike other
     * domains, HA's top-level "state" field for number entities is the
     * value itself (a numeric string), not "on"/"off".
     */
    NumberState get_number_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/number/set_value with
     * {"entity_id": entity_id, "value": value}
     */
    bool set_number_value(const char* base_url, const char* token,
                           const char* entity_id, float value);
```

- [ ] **Step 2: Add implementations to `ha_client.cpp`**

Add this block at the end of the `HA_CLIENT` namespace (after the existing `set_light_effect` function, before the closing `}` of the namespace). Note `get_number_state` needs `<cstdlib>` for `strtof` — add `#include <cstdlib>` to the top of the file alongside the existing includes.

```cpp
    SwitchState get_switch_state(const char* base_url, const char* token, const char* entity_id)
    {
        SwitchState result;

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
            ESP_LOGE(TAG, "get_switch_state failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_switch_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.is_on = (strcmp(state->valuestring, "on") == 0);
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    NumberState get_number_state(const char* base_url, const char* token, const char* entity_id)
    {
        NumberState result;

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
            ESP_LOGE(TAG, "get_number_state failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_number_state: JSON parse failed");
            return result;
        }

        /* For the number domain, top-level "state" IS the value, as a
           numeric string (e.g. "45.0"), not "on"/"off". */
        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.value = strtof(state->valuestring, nullptr);
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* min_attr = cJSON_GetObjectItem(attributes, "min");
            if (cJSON_IsNumber(min_attr))
            {
                result.min = (float)min_attr->valuedouble;
            }

            cJSON* max_attr = cJSON_GetObjectItem(attributes, "max");
            if (cJSON_IsNumber(max_attr))
            {
                result.max = (float)max_attr->valuedouble;
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_number_value(const char* base_url, const char* token,
                           const char* entity_id, float value)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"value\": %.2f}",
                 entity_id, value);

        return _post_json(base_url, token, "/api/services/number/set_value", body);
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
Add ha_client support for the switch and number domains

get_switch_state, get_number_state, set_number_value - for the
upcoming TV control app. Setting a switch reuses the existing
generic call_service(). Not yet used by any app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: TV config files

**Files:**
- Modify: `.gitignore`
- Create: `main/apps/app_lcd_test/tv_config.example.h` (committed template)
- Create: `main/apps/app_lcd_test/tv_config.h` (gitignored, real values)

**Interfaces:**
- Produces: `TV_WIFI_SSID`, `TV_WIFI_PASSWORD`, `TV_HA_BASE_URL`, `TV_HA_TOKEN`, `TV_SWITCH_ENTITY_ID`, `TV_VOLUME_ENTITY_ID` macros, consumed by Task 3.

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/app_set_brightness/fishtank_config.h` entry:

```
main/apps/app_lcd_test/tv_config.h
```

- [ ] **Step 2: Write the committed example file**

Write `main/apps/app_lcd_test/tv_config.example.h`:

```cpp
/**
 * @file tv_config.example.h
 * @brief Copy this file to tv_config.h (gitignored) and fill in real
 * values. tv_config.h is never committed.
 */
#pragma once

#define TV_WIFI_SSID          "YOUR_WIFI_SSID"
#define TV_WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"
#define TV_HA_BASE_URL        "http://YOUR_HA_HOST:8123"
#define TV_HA_TOKEN           "YOUR_LONG_LIVED_ACCESS_TOKEN"
#define TV_SWITCH_ENTITY_ID   "switch.your_entity"
#define TV_VOLUME_ENTITY_ID   "number.your_entity"
```

- [ ] **Step 3: Write the real (gitignored) config file**

Write `main/apps/app_lcd_test/tv_config.h` using the **same WiFi SSID/password and HA base URL/token already in `main/apps/app_sonos/sonos_config.h`** (same network, same HA instance — read that file to copy the values), with the TV's entity IDs:

```cpp
/**
 * @file tv_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define TV_WIFI_SSID          "<same SSID as sonos_config.h>"
#define TV_WIFI_PASSWORD      "<same password as sonos_config.h>"
#define TV_HA_BASE_URL        "<same base URL as sonos_config.h>"
#define TV_HA_TOKEN           "<same token as sonos_config.h>"
#define TV_SWITCH_ENTITY_ID   "switch.xiaomi_esprh1_0bc4_is_on"
#define TV_VOLUME_ENTITY_ID   "number.xiaomi_esprh1_0bc4_volume"
```

- [ ] **Step 4: Build to verify nothing else broke**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.`

- [ ] **Step 5: Commit — only the example file and .gitignore, never tv_config.h**

```bash
cd ~/Projects/M5Dial-UserDemo
git status
```
Confirm `main/apps/app_lcd_test/tv_config.h` does not appear as a file about to be added.

```bash
git add .gitignore main/apps/app_lcd_test/tv_config.example.h
git commit -m "$(cat <<'EOF'
Add TV config template (tv_config.example.h)

Real values live in the gitignored tv_config.h - same WiFi/HA
credentials already used by the Sonos/fish-tank-light apps, plus
the TV's switch and volume entity IDs.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Replace `app_lcd_test` internals

**Files:**
- Modify: `main/apps/app_lcd_test/app_lcd_test.h`
- Modify: `main/apps/app_lcd_test/app_lcd_test.cpp`
- Modify: `main/apps/app_lcd_test/gui/gui_lcd_test.h`
- Modify: `main/apps/app_lcd_test/gui/gui_lcd_test.cpp`

**Interfaces:**
- Consumes: `WIFI_CONNECT::connect/disconnect` (existing), `HA_CLIENT::SwitchState/get_switch_state`, `HA_CLIENT::NumberState/get_number_state/set_number_value`, `HA_CLIENT::call_service` (Task 1), `TV_*` macros (Task 2).
- Produces: nothing external — `LCD_Test` class name and `getGui()` signature stay unchanged, so the launcher (`launcher.cpp`'s `case 0: app_ptr = new MOONCAKE::USER_APP::LCD_Test;`) needs no changes.

- [ ] **Step 1: Replace `main/apps/app_lcd_test/gui/gui_lcd_test.h`**

```cpp
/**
 * @file gui_lcd_test.h
 * @brief Renderer for the TV control app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_LCD_TEST : public GUI_Base
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
         * @param volumePct 0-100, already normalized from the entity's actual min/max range
         * @param tvOn true if the TV is on (already inverted from the underlying switch)
         */
        void renderPage(int volumePct, bool tvOn);
};
```

- [ ] **Step 2: Replace `main/apps/app_lcd_test/gui/gui_lcd_test.cpp`**

```cpp
/**
 * @file gui_lcd_test.cpp
 */
#include "gui_lcd_test.h"
#include <cstdio>


void GUI_LCD_TEST::init()
{
}


void GUI_LCD_TEST::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_LCD_TEST::renderPage(int volumePct, bool tvOn)
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
    _canvas->drawCenterString("VOLUME", bubble.x, bubble.y - 30);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", volumePct);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 8);

    _canvas->setTextSize(1);
    _canvas->drawCenterString("TV", bubble.x, bubble.y + 34);

    /* On/off toggle button, below the bubble (bubble bottom edge is at
       bubble.y + bubble.height/2 = 190) */
    int btn_x = 120;
    int btn_y = 206;
    int btn_height = 24;
    const char* label = tvOn ? "ON" : "OFF";

    int text_w = _canvas->textWidth(label);
    int text_h = _canvas->fontHeight();
    int btn_width = text_w + 30;
    if (btn_width < 60) btn_width = 60;

    _canvas->fillSmoothRoundRect(btn_x - btn_width / 2, btn_y - btn_height / 2,
                                  btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(label, btn_x, btn_y - text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
```

- [ ] **Step 3: Replace `main/apps/app_lcd_test/app_lcd_test.h`**

```cpp
/**
 * @file app_lcd_test.h
 * @brief Controls a TV's power and volume through Home Assistant's REST
 * API. Power is inverted through a "sound bar mode" switch (ON = TV
 * off) - this file is the only place that inversion is visible; the
 * GUI and touch handling only ever deal with the TV's actual on/off
 * state. Touch: on/off toggle. Encoder: debounced volume. Encoder
 * button: quit (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_lcd_test.h"
#include "tv_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace LCD_TEST
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                bool tv_on = true;
                float volume = 50.0f;
                float volume_min = 0.0f;
                float volume_max = 100.0f;

                bool volume_dirty = false;
                uint32_t last_volume_change_ms = 0;
            };
        }

        class LCD_Test : public APP_BASE
        {
            private:
                const char* _tag = "lcd";

                void _handle_encoder();
                void _handle_touch();
                void _handle_volume_debounce();
                void _render();
                void _refresh_state();

            public:
                LCD_TEST::Data_t _data;
                GUI_LCD_TEST _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
```

- [ ] **Step 4: Replace `main/apps/app_lcd_test/app_lcd_test.cpp`**

```cpp
/**
 * @file app_lcd_test.cpp
 */
#include "app_lcd_test.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::LCD_TEST;


void LCD_Test::onSetup()
{
    setAppName("LCD_Test");
    setAllowBgRunning(false);

    LCD_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void LCD_Test::_refresh_state()
{
    HA_CLIENT::SwitchState sw = HA_CLIENT::get_switch_state(
        TV_HA_BASE_URL, TV_HA_TOKEN, TV_SWITCH_ENTITY_ID);

    if (!sw.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    /* Inverted: switch ON means the TV is OFF (sound-bar-only mode) */
    _data.tv_on = !sw.is_on;

    HA_CLIENT::NumberState num = HA_CLIENT::get_number_state(
        TV_HA_BASE_URL, TV_HA_TOKEN, TV_VOLUME_ENTITY_ID);

    if (!num.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.volume_min = num.min;
    _data.volume_max = num.max;
    if (!_data.volume_dirty)
    {
        _data.volume = num.value;
    }
}


void LCD_Test::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
    _render();

    bool connected = WIFI_CONNECT::connect(TV_WIFI_SSID, TV_WIFI_PASSWORD, 8000);
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


void LCD_Test::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    /* A full sweep of the range takes ~20 detents, regardless of the
       entity's actual scale */
    float step = (_data.volume_max - _data.volume_min) / 20.0f;
    float direction = (_data.hal->encoder.getDirection() < 1) ? 1.0f : -1.0f;

    _data.volume += direction * step;
    if (_data.volume < _data.volume_min) _data.volume = _data.volume_min;
    if (_data.volume > _data.volume_max) _data.volume = _data.volume_max;

    _data.volume_dirty = true;
    _data.last_volume_change_ms = millis();

    _render();
}


void LCD_Test::_handle_volume_debounce()
{
    if (!_data.volume_dirty)
        return;

    if (millis() - _data.last_volume_change_ms < 400)
        return;

    HA_CLIENT::set_number_value(TV_HA_BASE_URL, TV_HA_TOKEN, TV_VOLUME_ENTITY_ID, _data.volume);
    _data.volume_dirty = false;
}


void LCD_Test::_handle_touch()
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
        WIFI_CONNECT::disconnect();
        onCreate();
    }
    else if (_data.state == State::CONTROLLING && x >= 85 && x <= 155 && y >= 189 && y <= 223)
    {
        _data.tv_on = !_data.tv_on;

        /* Inverted: turning the TV on means turning the switch OFF */
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "switch",
                                 _data.tv_on ? "turn_off" : "turn_on", TV_SWITCH_ENTITY_ID);
        _refresh_state();
        _render();
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void LCD_Test::_render()
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
        float range = _data.volume_max - _data.volume_min;
        int volume_pct = (range > 0.0f)
            ? (int)((_data.volume - _data.volume_min) / range * 100.0f + 0.5f)
            : 0;

        _gui.renderPage(volume_pct, _data.tv_on);
    }
}


void LCD_Test::onRunning()
{
    _handle_encoder();
    _handle_volume_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void LCD_Test::onDestroy()
{
    _log("onDestroy");

    WIFI_CONNECT::disconnect();

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```

- [ ] **Step 5: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.`

- [ ] **Step 6: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_lcd_test
git commit -m "$(cat <<'EOF'
Repurpose LCD_Test into TV power+volume control via Home Assistant

Was: LCD color-test page cycler, no networking.
Now: CONNECTING/CONTROLLING/ERROR state machine (same shape as the
fish-tank-light and Sonos apps) controlling a TV's volume (encoder,
debounced, range read dynamically from the number entity's min/max)
and power (touch button). Power is inverted through a sound-bar-mode
switch (ON = TV off) - the inversion is fully contained in
_refresh_state/_handle_touch, never exposed in the GUI. Visual style
(white bubble) unchanged; class name and getGui() signature
unchanged too, so launcher.cpp needs no edits.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Flash and manually verify

**Files:** none (verification only)

- [ ] **Step 1: Flash the device**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
ls /dev/cu.*   # confirm the port hasn't changed
idf.py -p <PORT> flash
```
Expected: flashing completes, `Hash of data verified.` for both writes, device hard-resets.

- [ ] **Step 2: Manual verification checklist**

On the device, open the 1st launcher icon (same "LCD"/"TEST" tag as before):
- [ ] Shows "Connecting...", then the volume bubble (or an error screen)
- [ ] Bubble shows the TV's actual current volume as a percentage, and the ON/OFF button reflects its actual current power state (verified against the physical TV)
- [ ] Rotate the encoder → the TV's actual volume changes within ~400ms of stopping, doesn't flood HA with a request per detent
- [ ] Tap the ON/OFF button when TV is OFF → TV actually turns on, button changes to "ON" (confirms the switch inversion is correct, not backwards)
- [ ] Tap the ON/OFF button again → TV actually turns off, button changes to "OFF"
- [ ] Turn off WiFi (or take the device out of range) and reopen the app → error screen, not a hang; tapping it retries
- [ ] Encoder button press-and-release from connecting, controlling, and error states all return to the launcher

- [ ] **Step 3: Fix any issues found, rebuild, reflash, recheck**

Same iterative loop as every app built this session. If the power button turns out backwards (TV turns off when displayed as turning on), that means the `!` inversion in `_refresh_state`/`_handle_touch` needs removing rather than the touch coordinates being wrong — check that specifically before assuming it's a layout bug. Commit fixes:

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_lcd_test
git commit -m "$(cat <<'EOF'
Adjust TV control app after on-device testing

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Skip this commit if nothing needed adjusting.)

---

## Self-Review

**Spec coverage:**
- Power inversion via sound-bar-mode switch, hidden from UI — Task 3 (`_refresh_state`'s `!sw.is_on`, `_handle_touch`'s inverted service call, both commented explaining why; GUI only ever receives/shows `tvOn`). ✓
- Volume via `number` domain, range read dynamically — Task 1 (`get_number_state` parses `min`/`max`), Task 3 (`_handle_encoder`'s step computed from `volume_max - volume_min`, never hardcoded 0-100). ✓
- No MODE button (single adjustable value) — Task 3's GUI has only one button (ON/OFF), no mode toggle, unlike the fish-tank-light app. ✓
- Config reuses Sonos's WiFi/HA credentials, gitignored — Task 2. ✓
- Launcher untouched — Task 3 explicitly notes `LCD_Test` class name/`getGui()` signature don't change; no launcher file in any task's file list. ✓
- Testing/verification plan, specifically checking the inversion direction — Task 4. ✓

**Placeholder scan:** no TBD/TODO; all code blocks are complete, runnable code. Task 2's `tv_config.h` code block has `<same SSID as sonos_config.h>`-style placeholders, which is a real instruction to copy from an existing file (same pattern already used and executed successfully for `fishtank_config.h`), not a plan-writing shortcut.

**Type consistency:** `HA_CLIENT::SwitchState`/`NumberState` fields match between Task 1's definitions and Task 3's usage (`sw.ok`, `sw.is_on`, `num.ok`, `num.value`, `num.min`, `num.max`). `LCD_TEST::State` enum (`CONNECTING`/`CONTROLLING`/`ERROR`) used consistently through Task 3. `GUI_LCD_TEST::renderStatus`/`renderPage` signatures match between Task 3's header declaration, its `.cpp` definition, and its call sites in `_render()`.
