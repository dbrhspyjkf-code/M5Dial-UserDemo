# Fish Tank Light Brightness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repurpose `app_set_brightness` (launcher slot 3, "BRIGHTNESS"/"SET") from controlling the M5Dial's own backlight to controlling the fish tank light's brightness and on/off state in Home Assistant (`light.xiaomi_cn_931286672_m200_s_3_light`).

**Architecture:** Two new `ha_client` functions for the `light` domain, reusing the existing `wifi_connect_wrap`/`ha_client` utilities and `_make_client`/`_post_json` private helpers built for the Sonos app. `app_set_brightness`'s internals are fully replaced with a CONNECTING/CONTROLLING/ERROR state machine (same shape as `AppSonos`). Visual style (white bubble) and launcher slot/tag are unchanged.

**Tech Stack:** ESP-IDF v5.1.3, existing `esp_http_client`/`cJSON` via `ha_client`, C++.

## Global Constraints

- No unit test harness — "test" per task means `idf.py build` succeeds. Full behavior is verified once, at the end, by flashing and manually exercising every control against the real HA light.
- Encoder button press-and-release means "quit the app", unconditionally, in every state — project-wide convention.
- Config (`fishtank_config.h`) is compile-time, gitignored, never committed — same pattern as `sonos_config.h`.
- Build environment: `source ~/esp-idf-v5.1.3/export.sh` before any `idf.py` command. Flash port: check `ls /dev/cu.*` each time — confirmed `/dev/cu.usbmodem112401` as of the last Sonos flash, but it changes across sessions.
- Entity confirmed: `light.xiaomi_cn_931286672_m200_s_3_light`.
- WiFi/HA credentials are the same ones already used for the Sonos app (same network, same HA instance) — reuse those exact values when creating `fishtank_config.h`, don't ask the user again.

---

### Task 1: `ha_client` light-domain functions

**Files:**
- Modify: `main/apps/utilities/ha_client/ha_client.h`
- Modify: `main/apps/utilities/ha_client/ha_client.cpp`

**Interfaces:**
- Consumes: existing private helpers `_make_client()`, `_post_json()`, and existing public `call_service()` — all already in `ha_client.cpp`.
- Produces: `HA_CLIENT::LightState` struct and `get_light_state()`/`set_light_brightness()`/`set_light_power()` functions, consumed by Task 3.

- [ ] **Step 1: Add the new declarations to `ha_client.h`**

Add this block after the existing `MediaPlayerState` struct and its functions (keep everything already there unchanged):

```cpp
    struct LightState
    {
        bool ok = false;
        bool is_on = false;
        int brightness_pct = 0; // 0-100
    };

    /**
     * @brief GET /api/states/{entity_id} for a light entity
     */
    LightState get_light_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/light/turn_on with
     * {"entity_id": entity_id, "brightness_pct": brightness_pct}
     */
    bool set_light_brightness(const char* base_url, const char* token,
                               const char* entity_id, int brightness_pct);

    /**
     * @brief POST /api/services/light/turn_on or /turn_off (no extra fields)
     */
    bool set_light_power(const char* base_url, const char* token,
                          const char* entity_id, bool on);
```

- [ ] **Step 2: Add the implementations to `ha_client.cpp`**

Add this block at the end of the `HA_CLIENT` namespace (after the existing `set_volume` function, before the closing `}` of the namespace):

```cpp
    LightState get_light_state(const char* base_url, const char* token, const char* entity_id)
    {
        LightState result;

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
            ESP_LOGE(TAG, "get_light_state failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_light_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.is_on = (strcmp(state->valuestring, "on") == 0);
        }

        if (result.is_on)
        {
            cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
            if (attributes != nullptr)
            {
                cJSON* brightness = cJSON_GetObjectItem(attributes, "brightness");
                if (cJSON_IsNumber(brightness))
                {
                    /* HA's light.brightness is 0-255; convert to 0-100 */
                    result.brightness_pct = (brightness->valueint * 100 + 127) / 255;
                }
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_light_brightness(const char* base_url, const char* token,
                               const char* entity_id, int brightness_pct)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"brightness_pct\": %d}",
                 entity_id, brightness_pct);

        return _post_json(base_url, token, "/api/services/light/turn_on", body);
    }


    bool set_light_power(const char* base_url, const char* token,
                          const char* entity_id, bool on)
    {
        return call_service(base_url, token, "light", on ? "turn_on" : "turn_off", entity_id);
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
Add ha_client functions for the light domain

get_light_state/set_light_brightness/set_light_power, for the
upcoming fish tank light control app. Reuses the existing
_make_client/_post_json helpers and call_service. Not yet used by
any app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Fish tank config files

**Files:**
- Modify: `.gitignore`
- Create: `main/apps/app_set_brightness/fishtank_config.example.h` (committed template)
- Create: `main/apps/app_set_brightness/fishtank_config.h` (gitignored, real values)

**Interfaces:**
- Produces: `FISHTANK_WIFI_SSID`, `FISHTANK_WIFI_PASSWORD`, `FISHTANK_HA_BASE_URL`, `FISHTANK_HA_TOKEN`, `FISHTANK_ENTITY_ID` macros, consumed by Task 3.

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/app_sonos/sonos_config.h` entry:

```
main/apps/app_set_brightness/fishtank_config.h
```

- [ ] **Step 2: Write the committed example file**

Write `main/apps/app_set_brightness/fishtank_config.example.h`:

```cpp
/**
 * @file fishtank_config.example.h
 * @brief Copy this file to fishtank_config.h (gitignored) and fill in
 * real values. fishtank_config.h is never committed.
 */
#pragma once

#define FISHTANK_WIFI_SSID       "YOUR_WIFI_SSID"
#define FISHTANK_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define FISHTANK_HA_BASE_URL     "http://YOUR_HA_HOST:8123"
#define FISHTANK_HA_TOKEN        "YOUR_LONG_LIVED_ACCESS_TOKEN"
#define FISHTANK_ENTITY_ID       "light.your_entity"
```

- [ ] **Step 3: Write the real (gitignored) config file**

Write `main/apps/app_set_brightness/fishtank_config.h` using the **same WiFi SSID/password and HA base URL/token already in `main/apps/app_sonos/sonos_config.h`** (same network, same HA instance — read that file to copy the values rather than asking the user again), with the fish tank light's entity ID:

```cpp
/**
 * @file fishtank_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define FISHTANK_WIFI_SSID       "<same SSID as sonos_config.h>"
#define FISHTANK_WIFI_PASSWORD   "<same password as sonos_config.h>"
#define FISHTANK_HA_BASE_URL     "<same base URL as sonos_config.h>"
#define FISHTANK_HA_TOKEN        "<same token as sonos_config.h>"
#define FISHTANK_ENTITY_ID       "light.xiaomi_cn_931286672_m200_s_3_light"
```

- [ ] **Step 4: Build to verify nothing else broke**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.`

- [ ] **Step 5: Commit — only the example file and .gitignore, never fishtank_config.h**

```bash
cd ~/Projects/M5Dial-UserDemo
git status
```
Confirm `main/apps/app_set_brightness/fishtank_config.h` does not appear as a file about to be added.

```bash
git add .gitignore main/apps/app_set_brightness/fishtank_config.example.h
git commit -m "$(cat <<'EOF'
Add fish tank light config template (fishtank_config.example.h)

Real values live in the gitignored fishtank_config.h — same WiFi/HA
credentials already used by the Sonos app, plus the fish tank
light's entity ID.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Replace `app_set_brightness` internals

**Files:**
- Modify: `main/apps/app_set_brightness/app_set_brightness.h`
- Modify: `main/apps/app_set_brightness/app_set_brightness.cpp`
- Modify: `main/apps/app_set_brightness/gui/gui_set_brightness.h`
- Modify: `main/apps/app_set_brightness/gui/gui_set_brightness.cpp`

**Interfaces:**
- Consumes: `WIFI_CONNECT::connect/disconnect` (existing), `HA_CLIENT::LightState/get_light_state/set_light_brightness/set_light_power` (Task 1), `FISHTANK_*` macros (Task 2).
- Produces: nothing new externally — `Set_Brightness` class name and `getGui()` signature are unchanged, so the launcher (`launcher.cpp`'s `case 3: app_ptr = new MOONCAKE::USER_APP::Set_Brightness;`) needs no changes at all.

- [ ] **Step 1: Replace `main/apps/app_set_brightness/gui/gui_set_brightness.h`**

```cpp
/**
 * @file gui_set_brightness.h
 * @brief Renderer for the fish tank light control app. Pure display —
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
         * @brief Render the normal brightness-control screen
         *
         * @param brightnessPct 0-100
         * @param lightOn current on/off state (affects the toggle button label)
         */
        void renderPage(int brightnessPct, bool lightOn);
};
```

- [ ] **Step 2: Replace `main/apps/app_set_brightness/gui/gui_set_brightness.cpp`**

```cpp
/**
 * @file gui_set_brightness.cpp
 */
#include "gui_set_brightness.h"
#include <cstdio>


void GUI_SetBrightness::init()
{
}


void GUI_SetBrightness::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_SetBrightness::renderPage(int brightnessPct, bool lightOn)
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

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", brightnessPct);

    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 56);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("FISH LIGHT", bubble.x, bubble.y + 26);

    /* On/off toggle button, below the bubble (bubble bottom edge is at
       bubble.y + bubble.height/2 = 190) */
    int btn_x = 120;
    int btn_y = 202;
    int btn_height = 24;
    const char* label = lightOn ? "ON" : "OFF";

    _canvas->setTextSize(1);
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

- [ ] **Step 3: Replace `main/apps/app_set_brightness/app_set_brightness.h`**

```cpp
/**
 * @file app_set_brightness.h
 * @brief Controls the fish tank light's brightness and on/off state
 * through Home Assistant's REST API. Touch: on/off toggle. Encoder:
 * debounced brightness. Encoder button: quit (project-wide convention).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_set_brightness.h"
#include "fishtank_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace SET_BRIGHTNESS
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                int brightness_pct = 50;
                bool light_on = true;

                bool brightness_dirty = false;
                uint32_t last_brightness_change_ms = 0;
            };
        }

        class Set_Brightness : public APP_BASE
        {
            private:
                const char* _tag = "brightness";

                void _handle_encoder();
                void _handle_touch();
                void _handle_brightness_debounce();
                void _render();
                void _refresh_state();

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
```

- [ ] **Step 4: Replace `main/apps/app_set_brightness/app_set_brightness.cpp`**

```cpp
/**
 * @file app_set_brightness.cpp
 */
#include "app_set_brightness.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::SET_BRIGHTNESS;


void Set_Brightness::onSetup()
{
    setAppName("Set_Brightness");
    setAllowBgRunning(false);

    SET_BRIGHTNESS::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void Set_Brightness::_refresh_state()
{
    HA_CLIENT::LightState light = HA_CLIENT::get_light_state(
        FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN, FISHTANK_ENTITY_ID);

    if (!light.ok)
    {
        _data.state = State::ERROR;
        _data.error_message = "HA unreachable";
        return;
    }

    _data.light_on = light.is_on;
    if (!_data.brightness_dirty)
    {
        _data.brightness_pct = light.brightness_pct;
    }
}


void Set_Brightness::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
    _render();

    bool connected = WIFI_CONNECT::connect(FISHTANK_WIFI_SSID, FISHTANK_WIFI_PASSWORD, 8000);
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


void Set_Brightness::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int step = (_data.hal->encoder.getDirection() < 1) ? 5 : -5;
    _data.brightness_pct += step;
    if (_data.brightness_pct < 0) _data.brightness_pct = 0;
    if (_data.brightness_pct > 100) _data.brightness_pct = 100;

    _data.brightness_dirty = true;
    _data.last_brightness_change_ms = millis();

    _render();
}


void Set_Brightness::_handle_brightness_debounce()
{
    if (!_data.brightness_dirty)
        return;

    if (millis() - _data.last_brightness_change_ms < 400)
        return;

    HA_CLIENT::set_light_brightness(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN,
                                     FISHTANK_ENTITY_ID, _data.brightness_pct);
    _data.brightness_dirty = false;
}


void Set_Brightness::_handle_touch()
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
    else if (_data.state == State::CONTROLLING && x >= 85 && x <= 155 && y >= 185 && y <= 219)
    {
        _data.light_on = !_data.light_on;
        HA_CLIENT::set_light_power(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN,
                                    FISHTANK_ENTITY_ID, _data.light_on);
        _refresh_state();
        _render();
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void Set_Brightness::_render()
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
        _gui.renderPage(_data.brightness_pct, _data.light_on);
    }
}


void Set_Brightness::onRunning()
{
    _handle_encoder();
    _handle_brightness_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void Set_Brightness::onDestroy()
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
git add main/apps/app_set_brightness
git commit -m "$(cat <<'EOF'
Repurpose Set_Brightness into fish tank light control via Home Assistant

Was: local display backlight control, no networking.
Now: CONNECTING/CONTROLLING/ERROR state machine (same shape as
AppSonos) controlling the fish tank light's brightness (encoder,
debounced) and on/off state (new touch button). Visual style
(white bubble) and launcher slot/tag unchanged - class name and
getGui() signature unchanged too, so launcher.cpp needs no edits.

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

On the device, open the 4th launcher icon (same "BRIGHTNESS"/"SET" tag as before):
- [ ] Shows "Connecting...", then the brightness bubble (or an error screen)
- [ ] Bubble shows the fish tank light's actual current brightness as a percentage, and the ON/OFF button reflects its actual current power state
- [ ] Rotate the encoder → the fish tank light's actual brightness changes within ~400ms of stopping, doesn't flood HA with a request per detent
- [ ] Tap the ON/OFF button → the fish tank light actually turns on/off, and the displayed brightness updates to match (e.g. reads something sensible after turning back on, not stuck at 0%)
- [ ] Turn off WiFi (or take the device out of range) and reopen the app → error screen, not a hang; tapping it retries
- [ ] Encoder button press-and-release from connecting, controlling, and error states all return to the launcher

- [ ] **Step 3: Fix any issues found, rebuild, reflash, recheck**

Same iterative loop as the Timer/Sonos apps' final tasks — adjust the on/off button's touch-zone/layout constants in `gui_set_brightness.cpp`/`app_set_brightness.cpp` based on what's actually observed (the button sits close to both the bubble bottom and the quit indicator — this is the kind of spot that needed adjustment for every app built this session). Commit fixes:

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_set_brightness
git commit -m "$(cat <<'EOF'
Adjust fish tank light app after on-device testing

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Skip this commit if nothing needed adjusting.)

---

## Self-Review

**Spec coverage:**
- Entity `light.xiaomi_cn_931286672_m200_s_3_light` — Task 2. ✓
- On/off touch control — Task 3 (`_handle_touch`'s on/off branch, `set_light_power`). ✓
- Visual style unchanged (white bubble) — Task 3's `gui_set_brightness.cpp` keeps the exact bubble geometry from the original file. ✓
- Launcher untouched — Task 3 explicitly notes `Set_Brightness` class name/`getGui()` signature don't change, so `launcher.cpp`'s existing `case 3` needs no edit; confirmed no launcher file appears in any task's file list. ✓
- CONNECTING/CONTROLLING/ERROR state machine, same shape as `AppSonos` — Task 3. ✓
- `ha_client` light functions — Task 1. ✓
- Config reuses Sonos's WiFi/HA credentials, gitignored — Task 2. ✓
- Testing/verification plan — Task 4. ✓

**Placeholder scan:** no TBD/TODO; all code blocks are complete, runnable code. Task 2's `fishtank_config.h` code block has `<same SSID as sonos_config.h>`-style placeholders, but that's a real instruction to copy from an existing file, not a plan-writing shortcut — Step 3 explicitly says to read `sonos_config.h` for the actual values.

**Type consistency:** `HA_CLIENT::LightState` fields (`ok`, `is_on`, `brightness_pct`) match between Task 1's definition and Task 3's usage (`light.ok`, `light.is_on`, `light.brightness_pct`). `SET_BRIGHTNESS::State` enum (`CONNECTING`/`CONTROLLING`/`ERROR`) used consistently through Task 3. `GUI_SetBrightness::renderStatus`/`renderPage` signatures match between Task 3's header declaration, its `.cpp` definition, and its call sites in `_render()`.
