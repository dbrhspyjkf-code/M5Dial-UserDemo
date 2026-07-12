# RFID Tap-to-Turn-Off + Persistent WiFi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tapping a specific RFID card (the user's phone, S/N `355473325619`) against the M5Dial at any time — not just while an app is open — turns off 4 switches and a fan via Home Assistant, by moving RFID scanning and WiFi connection to boot-time singletons instead of per-app lifecycle.

**Architecture:** New `RFID_SERVICE` module owns the RC522 handle from boot onward and handles the match+action logic directly inside the existing (already-safe-for-blocking-calls) scan event callback. `main.cpp` connects WiFi once at boot instead of leaving that to each app. `app_rfid_test` becomes a thin viewer of `RFID_SERVICE`'s state instead of owning the RC522 handle itself. The 3 existing HA-backed apps stop disconnecting WiFi on close.

**Tech Stack:** ESP-IDF v5.1.3, existing `rc522` component, existing `wifi_connect_wrap`/`ha_client`, C++.

## Global Constraints

- No unit test harness — "test" per task means `idf.py build` succeeds. Full behavior is verified once, at the end, by flashing and tapping the actual phone card against the reader.
- Config is compile-time, gitignored, never committed — same pattern as every other app's config this session.
- Build environment: `source ~/esp-idf-v5.1.3/export.sh` before any `idf.py` command. Flash port: check `ls /dev/cu.*` each time — confirmed `/dev/cu.usbmodem11401` as of the last flash, but it changes across sessions.
- The RC522 scan event callback (`RC522_EVENT_TAG_SCANNED`) already runs in a safe FreeRTOS task context today (confirmed: the existing `app_rfid_test.cpp` callback already does buzzer + full-screen SPI redraw directly inside it, in production) — blocking HTTP calls inside it are safe, no additional queueing/threading needed.
- WiFi/HA credentials are the same ones already used for Sonos/fish-tank-light/TV (same network, same HA instance) — reuse those exact values when creating `rfid_service_config.h`, don't ask the user again.

---

### Task 1: `RFID_SERVICE` module

**Files:**
- Create: `main/apps/utilities/rfid_service/rfid_service.h`
- Create: `main/apps/utilities/rfid_service/rfid_service.cpp`
- Create: `main/apps/utilities/rfid_service/rfid_service_config.example.h` (committed template)
- Create: `main/apps/utilities/rfid_service/rfid_service_config.h` (gitignored, real values)
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `HAL::HAL*` (for `hal->buzz.tone()`), `HA_CLIENT::call_service()` (existing), `rc522.h` (existing component, same API already used in `app_rfid_test.cpp`).
- Produces: `RFID_SERVICE::init(HAL::HAL* hal)` and `RFID_SERVICE::last_scanned_sn() -> uint64_t`, consumed by Task 2 (`main.cpp`) and Task 3 (`app_rfid_test`).

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/app_lcd_test/tv_config.h` entry:

```
main/apps/utilities/rfid_service/rfid_service_config.h
```

- [ ] **Step 2: Write the committed example config**

```bash
mkdir -p ~/Projects/M5Dial-UserDemo/main/apps/utilities/rfid_service
```

Write `main/apps/utilities/rfid_service/rfid_service_config.example.h`:

```cpp
/**
 * @file rfid_service_config.example.h
 * @brief Copy this file to rfid_service_config.h (gitignored) and fill
 * in real values. rfid_service_config.h is never committed.
 */
#pragma once

#define RFID_WIFI_SSID       "YOUR_WIFI_SSID"
#define RFID_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define RFID_HA_BASE_URL     "http://YOUR_HA_HOST:8123"
#define RFID_HA_TOKEN        "YOUR_LONG_LIVED_ACCESS_TOKEN"

/* The card that triggers the turn-off action (decimal serial number,
   as shown by app_rfid_test when you scan it) */
#define RFID_PHONE_CARD_SN   0ULL

#define RFID_SWITCH_1   "switch.your_entity_1"
#define RFID_SWITCH_2   "switch.your_entity_2"
#define RFID_SWITCH_3   "switch.your_entity_3"
#define RFID_SWITCH_4   "switch.your_entity_4"
#define RFID_FAN_1      "fan.your_entity"
```

- [ ] **Step 3: Write the real (gitignored) config**

Write `main/apps/utilities/rfid_service/rfid_service_config.h` using the same WiFi/HA credentials already in `main/apps/app_sonos/sonos_config.h`, the confirmed phone card S/N, and the 5 confirmed entity IDs:

```cpp
/**
 * @file rfid_service_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define RFID_WIFI_SSID       "<same SSID as sonos_config.h>"
#define RFID_WIFI_PASSWORD   "<same password as sonos_config.h>"
#define RFID_HA_BASE_URL     "<same base URL as sonos_config.h>"
#define RFID_HA_TOKEN        "<same token as sonos_config.h>"

#define RFID_PHONE_CARD_SN   355473325619ULL

#define RFID_SWITCH_1   "switch.xiaomi_cn_945886502_w1_on_p_2_1"    // 餐厅灯
#define RFID_SWITCH_2   "switch.xiaomi_cn_945769368_w1_on_p_2_1"    // 书台灯
#define RFID_SWITCH_3   "switch.xiaomi_cn_2143401838_w2_on_p_3_1"   // 筒灯
#define RFID_SWITCH_4   "switch.xiaomi_cn_2143401838_w2_on_p_2_1"   // 吸顶灯
#define RFID_FAN_1      "fan.dmaker_cn_740412216_p5c_s_2_fan"       // 风扇
```

- [ ] **Step 4: Write `rfid_service.h`**

```cpp
/**
 * @file rfid_service.h
 * @brief Boot-time singleton that owns the RC522 reader for the whole
 * device's lifetime (not tied to any app's onCreate/onDestroy). Tapping
 * the configured phone card triggers a Home Assistant turn-off action
 * regardless of which app, if any, is currently open.
 */
#pragma once
#include <cstdint>
#include "../../../hal/hal.h"

namespace RFID_SERVICE
{
    /**
     * @brief Create and start the RC522 reader, register the scan event
     * handler. Call exactly once, at boot, before the launcher starts.
     */
    void init(HAL::HAL* hal);

    /**
     * @brief The most recently scanned card's serial number this boot,
     * or 0 if nothing has been scanned yet.
     */
    uint64_t last_scanned_sn();
}
```

- [ ] **Step 5: Write `rfid_service.cpp`**

```cpp
/**
 * @file rfid_service.cpp
 */
#include "rfid_service.h"
#include "rfid_service_config.h"
#include "../ha_client/ha_client.h"
#include <rc522.h>
#include <cstdio>


namespace RFID_SERVICE
{
    static const char* TAG = "rfid_service";

    static HAL::HAL* s_hal = nullptr;
    static rc522_handle_t s_rc522_handle;
    static uint64_t s_last_scanned_sn = 0;

    static void _turn_off_everything()
    {
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_1);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_2);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_3);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_4);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "fan", "turn_off", RFID_FAN_1);
    }

    static void _event_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
    {
        if (event_id != RC522_EVENT_TAG_SCANNED)
            return;

        rc522_event_data_t* data = (rc522_event_data_t*)event_data;
        rc522_tag_t* tag = (rc522_tag_t*)data->ptr;

        s_last_scanned_sn = tag->serial_number;

        if (s_hal != nullptr)
        {
            s_hal->buzz.tone(4000, 100);
        }

        if (s_last_scanned_sn == RFID_PHONE_CARD_SN)
        {
            _turn_off_everything();
        }
    }

    void init(HAL::HAL* hal)
    {
        s_hal = hal;

        rc522_config_t config;
        memset(&config, 0, sizeof(rc522_config_t));
        config.transport = RC522_TRANSPORT_I2C;
        config.i2c.port = I2C_NUM_0;

        rc522_create(&config, &s_rc522_handle);
        rc522_register_events(s_rc522_handle, RC522_EVENT_ANY, _event_handler, nullptr);
        rc522_start(s_rc522_handle);
    }

    uint64_t last_scanned_sn()
    {
        return s_last_scanned_sn;
    }
}
```

- [ ] **Step 6: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.` (new files picked up automatically by the `apps/*.cpp` glob; nothing calls `RFID_SERVICE::init()` yet, so this only checks it compiles standalone).

- [ ] **Step 7: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add .gitignore main/apps/utilities/rfid_service
git commit -m "$(cat <<'EOF'
Add RFID_SERVICE: boot-time RC522 singleton with tap-to-turn-off

Owns the RC522 handle independent of any app's lifecycle. Tapping
the configured phone card calls 5 Home Assistant turn_off services.
Not yet called from main.cpp - nothing initializes it yet.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Wire persistent WiFi + RFID_SERVICE into `main.cpp`

**Files:**
- Modify: `main/main.cpp`

**Interfaces:**
- Consumes: `WIFI_CONNECT::connect()` (existing), `RFID_SERVICE::init()` (Task 1).

- [ ] **Step 1: Add the includes and boot-time calls**

In `main/main.cpp`, add these includes near the top, alongside the existing ones:

```cpp
#include "apps/utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "apps/utilities/rfid_service/rfid_service.h"
#include "apps/utilities/rfid_service/rfid_service_config.h"
```

Add this right after `hal.init();` and before the factory-test-mode check:

```cpp
    /* Persistent WiFi + RFID scanning, from boot onward, independent of
       which app (if any) is open */
    WIFI_CONNECT::connect(RFID_WIFI_SSID, RFID_WIFI_PASSWORD, 8000);
    RFID_SERVICE::init(&hal);
```

So `app_main` starts with:
```cpp
extern "C" void app_main(void)
{
    HAL::HAL hal;

    /* Hardware init */
    hal.init();

    /* Persistent WiFi + RFID scanning, from boot onward, independent of
       which app (if any) is open */
    WIFI_CONNECT::connect(RFID_WIFI_SSID, RFID_WIFI_PASSWORD, 8000);
    RFID_SERVICE::init(&hal);

    // HAL::encoder_test(hal);
    // HAL::tp_test(hal);
    // HAL::rtc_test(hal);

/* Check factory test mode */
#ifdef ENABLE_FACTORY_TEST
    ...
```
(everything below `hal.init()` that was already there stays unchanged, just shifted down by these 5 new lines).

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
git add main/main.cpp
git commit -m "$(cat <<'EOF'
Connect WiFi and start RFID_SERVICE at boot

Both now run for the device's whole lifetime instead of being
started/stopped per-app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `app_rfid_test` becomes a viewer of `RFID_SERVICE`

**Files:**
- Modify: `main/apps/app_rfid_test/app_rfid_test.h`
- Modify: `main/apps/app_rfid_test/app_rfid_test.cpp`

**Interfaces:**
- Consumes: `RFID_SERVICE::last_scanned_sn()` (Task 1).
- Produces: nothing external — `RFID_Test` class name and `getGui()` signature stay unchanged, no launcher changes needed.

- [ ] **Step 1: Replace `main/apps/app_rfid_test/app_rfid_test.h`**

```cpp
/**
 * @file app_rfid_test.h
 * @brief Thin viewer of RFID_SERVICE's scan state - does NOT own an
 * RC522 handle itself (RFID_SERVICE owns the one shared handle, created
 * once at boot in main.cpp, independent of this app's lifecycle).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_rfid_test.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace APP_RFID_TEST
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                uint64_t displayed_sn = 0;
            };
        }

        class RFID_Test : public APP_BASE
        {
            private:
                const char* _tag = "rfid";

            public:
                APP_RFID_TEST::Data_t _data;
                GUI_RFID_Test _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };

    }
}
```

- [ ] **Step 2: Replace `main/apps/app_rfid_test/app_rfid_test.cpp`**

```cpp
/**
 * @file app_rfid_test.cpp
 */
#include "app_rfid_test.h"
#include "../common_define.h"
#include "../utilities/rfid_service/rfid_service.h"


using namespace MOONCAKE::USER_APP;


void RFID_Test::onSetup()
{
    setAppName("RFID_Test");
    setAllowBgRunning(false);

    APP_RFID_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void RFID_Test::onCreate()
{
    _log("onCreate");

    _data.displayed_sn = RFID_SERVICE::last_scanned_sn();
    if (_data.displayed_sn != 0)
    {
        _gui.renderPage(_data.displayed_sn);
    }
}


void RFID_Test::onRunning()
{
    uint64_t current = RFID_SERVICE::last_scanned_sn();
    if (current != 0 && current != _data.displayed_sn)
    {
        _data.displayed_sn = current;
        _gui.renderPage(_data.displayed_sn);
    }

    /* If button pressed */
    if (!_data.hal->encoder.btn.read())
    {
        /* Hold until button release */
        while (!_data.hal->encoder.btn.read())
            delay(5);

        /* Bye */
        destroyApp();
    }
}


void RFID_Test::onDestroy()
{
    _log("onDestroy");
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
git add main/apps/app_rfid_test/app_rfid_test.h main/apps/app_rfid_test/app_rfid_test.cpp
git commit -m "$(cat <<'EOF'
Turn app_rfid_test into a viewer of RFID_SERVICE's scan state

No longer creates/destroys its own RC522 handle - that now belongs
to RFID_SERVICE, running from boot. Opening this app just polls and
displays RFID_SERVICE::last_scanned_sn().

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Stop the 3 HA apps from disconnecting WiFi on close

**Files:**
- Modify: `main/apps/app_sonos/app_sonos.cpp`
- Modify: `main/apps/app_set_brightness/app_set_brightness.cpp`
- Modify: `main/apps/app_lcd_test/app_lcd_test.cpp`

**Interfaces:** none new — this only removes a line from each `onDestroy()`.

- [ ] **Step 1: Remove the disconnect call from each `onDestroy()`**

In `main/apps/app_sonos/app_sonos.cpp`, change:
```cpp
void AppSonos::onDestroy()
{
    _log("onDestroy");

    WIFI_CONNECT::disconnect();

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with
       (same fix already applied for AppTimer). */
    _data.hal->canvas->setTextSize(1);
}
```
to:
```cpp
void AppSonos::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with
       (same fix already applied for AppTimer). */
    _data.hal->canvas->setTextSize(1);
}
```

In `main/apps/app_set_brightness/app_set_brightness.cpp`, change:
```cpp
void Set_Brightness::onDestroy()
{
    _log("onDestroy");

    WIFI_CONNECT::disconnect();

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```
to:
```cpp
void Set_Brightness::onDestroy()
{
    _log("onDestroy");

    /* WiFi is connected once at boot (main.cpp) and stays up for
       RFID_SERVICE - this app must not disconnect it on close. */

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```

In `main/apps/app_lcd_test/app_lcd_test.cpp`, change:
```cpp
void LCD_Test::onDestroy()
{
    _log("onDestroy");

    WIFI_CONNECT::disconnect();

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```
to:
```cpp
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

(The `#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"` line stays in all 3 files — each still calls `WIFI_CONNECT::connect()` in `onCreate()`, which is now a fast no-op since WiFi is already up, not something to remove.)

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
git add main/apps/app_sonos/app_sonos.cpp main/apps/app_set_brightness/app_set_brightness.cpp main/apps/app_lcd_test/app_lcd_test.cpp
git commit -m "$(cat <<'EOF'
Stop Sonos/fish-tank-light/TV apps from disconnecting WiFi on close

WiFi is now connected once at boot and kept up for RFID_SERVICE
(main.cpp). Each app's onCreate() still calls WIFI_CONNECT::connect(),
now a fast no-op since it's already connected - which also removes
the real "Connecting..." delay these apps used to show.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Flash and manually verify

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

- [ ] Right after boot, without opening any app, tap the phone card against the reader → buzzer sounds, and within a few seconds the 4 switches + fan actually turn off (check the real devices)
- [ ] Open `app_rfid_test` (launcher slot 2, "RFID"/"TEST") and tap any card → shows "TAG SCANNED" + the correct S/N, same UI as before this change
- [ ] Open Sonos → noticeably less/no "Connecting..." delay compared to before this feature
- [ ] Open fish-tank-light app → same, less/no delay
- [ ] Open TV app → same, less/no delay
- [ ] While one of those 3 apps is open, tap the phone card → the turn-off action still fires (confirms RFID_SERVICE keeps running independent of which app has the screen)
- [ ] Quit from every app still returns to the launcher normally

- [ ] **Step 3: Fix any issues found, rebuild, reflash, recheck**

Commit fixes:

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/utilities/rfid_service main/main.cpp main/apps/app_rfid_test
git commit -m "$(cat <<'EOF'
Adjust RFID tap-to-turn-off after on-device testing

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Skip this commit if nothing needed adjusting.)

---

## Self-Review

**Spec coverage:**
- RFID becomes a boot-time singleton, `app_rfid_test` becomes a viewer — Task 1, Task 3. ✓
- Exact-match against one hardcoded card — Task 1 (`s_last_scanned_sn == RFID_PHONE_CARD_SN`). ✓
- Buzz + 5 turn-off calls on match — Task 1 (`_event_handler`, `_turn_off_everything`). ✓
- WiFi connects once at boot, never disconnected — Task 2 (`main.cpp`), Task 4 (3 apps stop disconnecting). ✓
- Blocking HTTP calls safe inside the RC522 callback — addressed in Global Constraints, no additional threading code needed since the spec already established this is safe. ✓
- Testing/verification plan — Task 5. ✓

**Placeholder scan:** no TBD/TODO; all code blocks are complete, runnable code. Task 1's `rfid_service_config.h` code block has `<same SSID as sonos_config.h>`-style placeholders, matching the already-executed pattern from `fishtank_config.h`/`tv_config.h` — a real copy instruction, not a shortcut.

**Type consistency:** `RFID_SERVICE::init(HAL::HAL*)` / `last_scanned_sn() -> uint64_t` match between Task 1's header and Task 2/3's call sites. `RFID_Test`'s `Data_t::displayed_sn` (uint64_t) matches `RFID_SERVICE::last_scanned_sn()`'s return type in Task 3. `GUI_RFID_Test::renderPage(const uint64_t&)` (unchanged, pre-existing) still matches its call sites in Task 3's `app_rfid_test.cpp`.
