# TV Nav Remote Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a second "NAV" page (up/down/left/right/OK/back/menu) to the existing TV control app (`LCD_Test`/`GUI_LCD_TEST`), toggled from the existing VOLUME page via a new MODE button. Also drop the white bubble in favor of the flat themed-background style.

**Architecture:** New `Page` enum (`VOLUME`/`NAV`) added to `LCD_TEST::Data_t`. Touch handling branches on the current page. NAV page buttons call `HA_CLIENT::call_service(..., "button", "press", <entity>)` directly (already-existing generic function, no new HA_CLIENT code) with no state refresh afterward (button-domain entities are momentary, nothing to read back). Class names and launcher wiring stay unchanged.

**Tech Stack:** ESP-IDF v5.1.3, existing `HA_CLIENT::call_service`, LovyanGFX (`GUI_Base`).

## Global Constraints

- 7 new entity ID macros go in `tv_config.h`/`tv_config.example.h` (not a new config file — this extends the existing TV app, doesn't need new WiFi/HA credentials).
- No state refresh after NAV button presses — these are momentary `button` domain entities, not toggles.
- Encoder does nothing on the NAV page (only matters on the VOLUME page).
- Flat themed-background style (no white bubble) on both pages, matching Timer/Stock.

---

### Task 1: Add the 7 nav entity IDs to `tv_config.h` / `.example.h`

**Files:**
- Modify: `main/apps/app_lcd_test/tv_config.h`
- Modify: `main/apps/app_lcd_test/tv_config.example.h`

**Interfaces:**
- Produces: `TV_NAV_UP_ENTITY_ID`, `TV_NAV_DOWN_ENTITY_ID`, `TV_NAV_LEFT_ENTITY_ID`, `TV_NAV_RIGHT_ENTITY_ID`, `TV_NAV_OK_ENTITY_ID`, `TV_NAV_BACK_ENTITY_ID`, `TV_NAV_MENU_ENTITY_ID` macros. Consumed by Task 2.

- [ ] **Step 1: Append to `main/apps/app_lcd_test/tv_config.h`**

```cpp
#define TV_NAV_UP_ENTITY_ID     "button.xiaomi_cn_629973618_esprh1_press_up_a_7_8"
#define TV_NAV_DOWN_ENTITY_ID   "button.xiaomi_cn_629973618_esprh1_press_down_a_7_9"
#define TV_NAV_LEFT_ENTITY_ID   "button.xiaomi_cn_629973618_esprh1_press_left_a_7_6"
#define TV_NAV_RIGHT_ENTITY_ID  "button.xiaomi_cn_629973618_esprh1_press_right_a_7_7"
#define TV_NAV_OK_ENTITY_ID     "button.xiaomi_cn_629973618_esprh1_press_ok_a_7_10"
#define TV_NAV_BACK_ENTITY_ID   "button.xiaomi_cn_629973618_esprh1_press_back_a_7_5"
#define TV_NAV_MENU_ENTITY_ID   "button.xiaomi_cn_629973618_esprh1_press_menu_a_7_3"
```

- [ ] **Step 2: Append the same macros (with placeholder values) to `main/apps/app_lcd_test/tv_config.example.h`**

```cpp
#define TV_NAV_UP_ENTITY_ID     "button.your_tv_press_up"
#define TV_NAV_DOWN_ENTITY_ID   "button.your_tv_press_down"
#define TV_NAV_LEFT_ENTITY_ID   "button.your_tv_press_left"
#define TV_NAV_RIGHT_ENTITY_ID  "button.your_tv_press_right"
#define TV_NAV_OK_ENTITY_ID     "button.your_tv_press_ok"
#define TV_NAV_BACK_ENTITY_ID   "button.your_tv_press_back"
#define TV_NAV_MENU_ENTITY_ID   "button.your_tv_press_menu"
```

- [ ] **Step 3: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.` (These macros aren't referenced yet, so this just confirms no syntax errors in the config files.)

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_lcd_test/tv_config.example.h
git commit -m "$(cat <<'EOF'
Add TV nav-remote entity IDs to tv_config

7 button-domain entities (up/down/left/right/ok/back/menu),
confirmed live against HA. tv_config.h itself is gitignored - only
the example template is committed here.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`tv_config.h` is gitignored — it won't show up as trackable, nothing more to commit for it.)

---

### Task 2: Add the NAV page to `app_lcd_test.h` / `.cpp`

**Files:**
- Modify: `main/apps/app_lcd_test/app_lcd_test.h`
- Modify: `main/apps/app_lcd_test/app_lcd_test.cpp`

**Interfaces:**
- Consumes: the 7 nav entity ID macros (Task 1), `HA_CLIENT::call_service` (existing).
- Produces: calls into `GUI_LCD_TEST::renderVolumePage(int volumePct, bool tvOn)` and `GUI_LCD_TEST::renderNavPage()` — signatures defined in Task 3 (renamed from the current single `renderPage`).

- [ ] **Step 1: Replace `main/apps/app_lcd_test/app_lcd_test.h`**

```cpp
/**
 * @file app_lcd_test.h
 * @brief Controls a TV's power, volume, and navigation (D-pad) through
 * Home Assistant's REST API. Power is inverted through a "sound bar
 * mode" switch (ON = TV off) - this file is the only place that
 * inversion is visible; the GUI and touch handling only ever deal with
 * the TV's actual on/off state. Two pages, toggled by touch: VOLUME
 * (power/volume) and NAV (up/down/left/right/ok/back/menu, all
 * momentary button-domain presses with nothing to read back). Encoder:
 * debounced volume, VOLUME page only. Encoder button: quit
 * (project-wide convention).
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
            enum class Page { VOLUME, NAV };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;
                Page page = Page::VOLUME;

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
                void _handle_touch_volume_page(int x, int y);
                void _handle_touch_nav_page(int x, int y);
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

- [ ] **Step 2: Replace `main/apps/app_lcd_test/app_lcd_test.cpp`**

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

    /* Render the control screen immediately with default values instead
       of a blank frame or a "Connecting..." screen - WiFi/HA_CLIENT's
       connection are already up from boot, so the real fetch below is
       normally fast enough that this briefly-stale render is barely
       noticeable, then gets replaced by the real state. */
    _data.state = State::CONTROLLING;
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
    if (_data.state != State::CONTROLLING || _data.page != Page::VOLUME)
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


void LCD_Test::_handle_touch_volume_page(int x, int y)
{
    if (y < 189 || y > 223)
        return;

    if (x >= 30 && x <= 118)
    {
        /* MODE button - switch to NAV page */
        _data.page = Page::NAV;
        _render();
    }
    else if (x >= 122 && x <= 210)
    {
        /* POWER button */
        _data.tv_on = !_data.tv_on;

        /* Inverted: turning the TV on means turning the switch OFF */
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "switch",
                                 _data.tv_on ? "turn_off" : "turn_on", TV_SWITCH_ENTITY_ID);
        _refresh_state();
        _render();
    }
}


void LCD_Test::_handle_touch_nav_page(int x, int y)
{
    /* D-pad cross, centered around (120, 110) */
    if (x >= 103 && x <= 137 && y >= 72 && y <= 96)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_UP_ENTITY_ID);
    }
    else if (x >= 103 && x <= 137 && y >= 124 && y <= 148)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_DOWN_ENTITY_ID);
    }
    else if (x >= 68 && x <= 100 && y >= 98 && y <= 122)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_LEFT_ENTITY_ID);
    }
    else if (x >= 140 && x <= 172 && y >= 98 && y <= 122)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_RIGHT_ENTITY_ID);
    }
    else if (x >= 103 && x <= 137 && y >= 98 && y <= 122)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_OK_ENTITY_ID);
    }
    else if (x >= 30 && x <= 82 && y >= 181 && y <= 205)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_BACK_ENTITY_ID);
    }
    else if (x >= 94 && x <= 146 && y >= 181 && y <= 205)
    {
        HA_CLIENT::call_service(TV_HA_BASE_URL, TV_HA_TOKEN, "button", "press", TV_NAV_MENU_ENTITY_ID);
    }
    else if (x >= 158 && x <= 210 && y >= 181 && y <= 205)
    {
        /* VOL button - switch back to VOLUME page */
        _data.page = Page::VOLUME;
        _render();
    }
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
        onCreate();
    }
    else if (_data.state == State::CONTROLLING)
    {
        if (_data.page == Page::VOLUME)
        {
            _handle_touch_volume_page(x, y);
        }
        else
        {
            _handle_touch_nav_page(x, y);
        }
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
    else if (_data.page == Page::NAV)
    {
        _gui.renderNavPage();
    }
    else
    {
        float range = _data.volume_max - _data.volume_min;
        int volume_pct = (range > 0.0f)
            ? (int)((_data.volume - _data.volume_min) / range * 100.0f + 0.5f)
            : 0;

        _gui.renderVolumePage(volume_pct, _data.tv_on);
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

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```

- [ ] **Step 3: Build to verify it compiles**

This will fail until Task 3 renames `renderPage` to `renderVolumePage` and adds `renderNavPage` - build after Task 3 instead. Skip building at the end of this task; proceed directly to Task 3.

---

### Task 3: Update `gui_lcd_test.h` / `.cpp` — flat style + NAV page

**Files:**
- Modify: `main/apps/app_lcd_test/gui/gui_lcd_test.h`
- Modify: `main/apps/app_lcd_test/gui/gui_lcd_test.cpp`

**Interfaces:**
- Consumes: nothing new (pure rendering).
- Produces: `GUI_LCD_TEST::renderStatus(line1, line2)` (unchanged signature, restyled flat), `GUI_LCD_TEST::renderVolumePage(int volumePct, bool tvOn)` (renamed from `renderPage`, restyled flat), `GUI_LCD_TEST::renderNavPage()` (new) — consumed by Task 2.

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
         * @brief Render the volume/power page
         */
        void renderVolumePage(int volumePct, bool tvOn);

        /**
         * @brief Render the D-pad navigation page
         */
        void renderNavPage();
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

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), 120, 115 - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), 120, 150);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_LCD_TEST::renderVolumePage(int volumePct, bool tvOn)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("VOLUME", 120, 60);

    char string_buffer[24];
    snprintf(string_buffer, sizeof(string_buffer), "%d%%", volumePct);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(string_buffer, 120, 100);

    _canvas->setTextSize(1);
    _canvas->drawCenterString("TV", 120, 148);

    /* MODE and POWER buttons, side by side */
    int btn_y = 206;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 78;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    const char* power_label = tvOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 162;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_LCD_TEST::renderNavPage()
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("NAV", 120, 50);

    /* D-pad cross, centered around (120, 110) */
    auto draw_dpad_button = [this](const char* label, int x, int y, int w, int h)
    {
        _canvas->fillSmoothRoundRect(x - w / 2, y - h / 2, w, h, 8, TFT_WHITE);
        _canvas->setTextColor(_theme_color);
        int text_h = _canvas->fontHeight();
        _canvas->drawCenterString(label, x, y - text_h / 2);
    };

    draw_dpad_button("^", 120, 84, 34, 24);
    draw_dpad_button("v", 120, 136, 34, 24);
    draw_dpad_button("<", 84, 110, 32, 24);
    draw_dpad_button(">", 156, 110, 32, 24);
    draw_dpad_button("OK", 120, 110, 34, 24);

    /* BACK / MENU / VOL row */
    draw_dpad_button("BACK", 56, 193, 52, 24);
    draw_dpad_button("MENU", 120, 193, 52, 24);
    draw_dpad_button("VOL", 184, 193, 52, 24);

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
git add main/apps/app_lcd_test/app_lcd_test.h main/apps/app_lcd_test/app_lcd_test.cpp main/apps/app_lcd_test/gui/gui_lcd_test.h main/apps/app_lcd_test/gui/gui_lcd_test.cpp
git commit -m "$(cat <<'EOF'
Add NAV page (D-pad) to TV control, drop the white bubble

Two pages now, toggled by touch: VOLUME (power/volume, unchanged
behavior) and NAV (up/down/left/right/OK/back/menu - all momentary
button-domain presses, no state to read back). Both pages restyled
to the flat themed-background convention already used by Timer/
Stock, dropping the white rounded-rect bubble. renderPage() renamed
to renderVolumePage() for clarity now that there are two pages.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Flash and manual verification

**Files:** none — device flash + on-device check.

- [ ] **Step 1: Check current device port**

```bash
ls /dev/cu.*
```

- [ ] **Step 2: Flash**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py -p <PORT> flash
```

- [ ] **Step 3: Manual verification checklist**

- VOLUME page: unchanged behavior (encoder adjusts volume, POWER toggles), now flat (no white bubble), plus a new MODE button switches to the NAV page.
- NAV page: tapping UP/DOWN/LEFT/RIGHT/OK/BACK/MENU actually moves the TV's on-screen selection / triggers the expected action (verify by watching the physical TV).
- NAV page's VOL button switches back to the VOLUME page.
- Touch zone boundaries feel right (no dead zones or accidental double-hits between adjacent D-pad buttons) - adjust pixel coordinates based on on-device photo feedback if needed, same iterative process used for every prior app's layout.
- Quitting (encoder button press-and-release) works from both pages.
- Tapping the phone RFID card while this app is open still turns off the lights/fan (RFID_SERVICE regression check).
- Leaving the device idle for 5 minutes on either page still turns the backlight off, and touching wakes it without also triggering a button underneath (IDLE_SCREEN regression check).
