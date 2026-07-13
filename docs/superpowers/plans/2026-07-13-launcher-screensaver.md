# Launcher Screensaver + NTP Time Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 30s idle on the Launcher's icon carousel shows a full-screen clock/date/weather screensaver, dismissed by any touch/encoder/button activity. RTC synced to real time via NTP once at boot.

**Architecture:** Two new one-shot utilities (`NTP_SYNC`, `WEATHER_CLIENT`) plus screensaver state/logic added directly to the existing `Launcher` class (it has no `GUI_Base` subclass — it draws straight to `hal->canvas`, same as its existing carousel rendering). Independent of `IDLE_SCREEN` (different timer, different trigger, different scope — only the Launcher, never inside an app).

**Tech Stack:** ESP-IDF v5.1.3 SNTP client (`esp_sntp.h`, part of the `lwip` component already used for WiFi/HTTP), existing PCF8563 RTC HAL, `esp_http_client` + cJSON (same one-shot-call pattern as `STOCK_CLIENT`).

## Global Constraints

- Screensaver only ever runs while `Launcher::onRunning()` is active (never inside an app) — this is automatically true since `_simple_app_manager` takes over the loop entirely while an app is open.
- Screensaver activity detection must use `hal->encoder.getCount()` directly, never `wasMoved()` — the Launcher's own carousel-scroll logic already calls `wasMoved(true)` in `_launcher_loop()`, and calling it a second time here would consume rotation before the carousel sees it (the exact bug `IDLE_SCREEN` was designed to avoid).
- **Critical HAL quirk**: `PCF8563::getTime()`/`setTime()` use a **non-POSIX** convention for `tm_year` — this HAL treats it as the actual full year (e.g. `2026`), not "years since 1900" like standard C `struct tm`. `time()`/`localtime_r()` (used to read the NTP-synced system clock) return standard POSIX `tm_year` (years since 1900, e.g. `126` for 2026). **`NTP_SYNC` must add 1900 to `localtime_r()`'s `tm_year` before calling `hal->rtc.setTime()`**, or the RTC will be set to a wildly wrong year (126 AD).

---

### Task 1: Create the `NTP_SYNC` utility

**Files:**
- Create: `main/apps/utilities/ntp_sync/ntp_sync.h`
- Create: `main/apps/utilities/ntp_sync/ntp_sync.cpp`

**Interfaces:**
- Produces: `NTP_SYNC::sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms)`. Consumed by Task 4 (`main.cpp`).

- [ ] **Step 1: Write `ntp_sync.h`**

```cpp
/**
 * @file ntp_sync.h
 * @brief One-shot SNTP time sync at boot, writing the result into the
 * device's battery-backed PCF8563 RTC so it's accurate from a real
 * time source instead of just whatever it last had.
 */
#pragma once
#include "../../../hal/hal.h"
#include <cstdint>

namespace NTP_SYNC
{
    /**
     * @brief Start SNTP, wait up to timeout_ms for it to complete, and
     * write the resulting time into the RTC. If it times out (no
     * network, bad NTP response), the RTC is left unchanged - this is
     * a best-effort accuracy improvement, not a hard dependency.
     */
    void sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms);
}
```

- [ ] **Step 2: Write `ntp_sync.cpp`**

```cpp
/**
 * @file ntp_sync.cpp
 */
#include "ntp_sync.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <ctime>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace NTP_SYNC
{
    static const char* TAG = "ntp_sync";

    void sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms)
    {
        setenv("TZ", "CST-8", 1);
        tzset();

        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_init();

        uint32_t waited_ms = 0;
        while (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && waited_ms < timeout_ms)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            waited_ms += 200;
        }

        if (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
        {
            ESP_LOGW(TAG, "NTP sync timed out after %u ms, RTC left unchanged", (unsigned)timeout_ms);
            return;
        }

        time_t now;
        time(&now);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        /* PCF8563::setTime() expects tm_year as the actual full year
           (e.g. 2026), not POSIX's "years since 1900" - localtime_r()
           gives us the POSIX convention, so it must be adjusted here. */
        struct tm rtc_time = timeinfo;
        rtc_time.tm_year = timeinfo.tm_year + 1900;

        hal->rtc.setTime(rtc_time);

        ESP_LOGI(TAG, "RTC synced: %04d-%02d-%02d %02d:%02d:%02d",
                 rtc_time.tm_year, rtc_time.tm_mon + 1, rtc_time.tm_mday,
                 rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
    }
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
git add main/apps/utilities/ntp_sync
git commit -m "$(cat <<'EOF'
Add NTP_SYNC: one-shot boot-time RTC sync via SNTP

Starts SNTP, waits up to a timeout for it to complete, then writes
the result into the PCF8563 RTC (adjusting tm_year from POSIX's
years-since-1900 to this HAL's actual-full-year convention - getting
this wrong would set the RTC to year 126). Times out silently if
there's no network - RTC just keeps whatever time it had. Not yet
called from anywhere.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Create the `WEATHER_CLIENT` utility

**Files:**
- Create: `main/apps/utilities/weather_client/weather_client.h`
- Create: `main/apps/utilities/weather_client/weather_client.cpp`

**Interfaces:**
- Produces: `WEATHER_CLIENT::WeatherInfo{ok, temp_c, condition}`, `WEATHER_CLIENT::get_weather(const char* base_url) -> WeatherInfo`. Consumed by Task 4 (`launcher.cpp`).

- [ ] **Step 1: Write `weather_client.h`**

```cpp
/**
 * @file weather_client.h
 * @brief One-shot HTTP client for the hermes-mcp-xiaozhi weather
 * endpoint. Same non-persistent-connection reasoning as STOCK_CLIENT -
 * low call frequency (once per screensaver activation), no auth
 * needed, small response.
 */
#pragma once
#include <string>

namespace WEATHER_CLIENT
{
    struct WeatherInfo
    {
        bool ok = false;
        std::string temp_c;
        std::string condition;
    };

    /**
     * @brief GET {base_url}/weather (default city, already configured
     * server-side). Returns ok=false on any failure.
     */
    WeatherInfo get_weather(const char* base_url);
}
```

- [ ] **Step 2: Write `weather_client.cpp`**

```cpp
/**
 * @file weather_client.cpp
 */
#include "weather_client.h"
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace WEATHER_CLIENT
{
    static const char* TAG = "weather_client";

    struct ResponseBuffer
    {
        char data[2048];
        int len = 0;
    };

    static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            auto* buf = (ResponseBuffer*)evt->user_data;
            int space = (int)sizeof(buf->data) - buf->len - 1;
            int copy_len = evt->data_len < space ? evt->data_len : space;
            if (copy_len > 0)
            {
                memcpy(buf->data + buf->len, evt->data, copy_len);
                buf->len += copy_len;
                buf->data[buf->len] = '\0';
            }
        }
        return ESP_OK;
    }

    WeatherInfo get_weather(const char* base_url)
    {
        WeatherInfo result;

        char url[128];
        snprintf(url, sizeof(url), "%s/weather", base_url);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000;
        config.event_handler = _http_event_handler;
        config.user_data = &resp_buf;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "get_weather failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_weather: JSON parse failed");
            return result;
        }

        cJSON* temp_c = cJSON_GetObjectItem(root, "temp_c");
        if (cJSON_IsString(temp_c)) result.temp_c = temp_c->valuestring;

        cJSON* condition = cJSON_GetObjectItem(root, "condition");
        if (cJSON_IsString(condition)) result.condition = condition->valuestring;

        cJSON* ok = cJSON_GetObjectItem(root, "ok");
        result.ok = cJSON_IsTrue(ok) && !result.temp_c.empty();

        cJSON_Delete(root);
        return result;
    }
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
git add main/apps/utilities/weather_client
git commit -m "$(cat <<'EOF'
Add WEATHER_CLIENT: one-shot fetch for the hermes-mcp-xiaozhi weather API

GET {base_url}/weather, parses temp_c/condition/ok. No persistent
connection - called once per screensaver activation, not
continuously polled. Not yet used - no caller until the launcher
screensaver.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Config file for the weather API base URL

**Files:**
- Create: `main/apps/launcher/screensaver_config.example.h`
- Create: `main/apps/launcher/screensaver_config.h` (gitignored)
- Modify: `.gitignore`

**Interfaces:**
- Produces: `WEATHER_API_BASE_URL` macro. Consumed by Task 4.

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/app_more_menu/stock_config.h` entry:

```
main/apps/launcher/screensaver_config.h
```

- [ ] **Step 2: Write the committed example config**

Write `main/apps/launcher/screensaver_config.example.h`:

```cpp
/**
 * @file screensaver_config.example.h
 * @brief Copy this file to screensaver_config.h (gitignored) and fill
 * in real values. screensaver_config.h is never committed.
 */
#pragma once

#define WEATHER_API_BASE_URL   "http://YOUR_HOST:8766"
```

- [ ] **Step 3: Write the real (gitignored) config**

Write `main/apps/launcher/screensaver_config.h`:

```cpp
/**
 * @file screensaver_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define WEATHER_API_BASE_URL   "http://192.168.1.200:8766"
```

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add .gitignore main/apps/launcher/screensaver_config.example.h
git commit -m "$(cat <<'EOF'
Add screensaver_config template for the launcher screensaver

Real screensaver_config.h is gitignored, not part of this commit.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Add the screensaver to `Launcher`

**Files:**
- Modify: `main/apps/launcher/launcher.h`
- Modify: `main/apps/launcher/launcher.cpp`

**Interfaces:**
- Consumes: `WEATHER_CLIENT::get_weather` (Task 2), `WEATHER_API_BASE_URL` (Task 3), `hal->rtc.getTime()` (existing).

- [ ] **Step 1: Add fields to `LAUNCHER::Data_t` and declare new methods**

In `main/apps/launcher/launcher.h`, change:

```cpp
        namespace LAUNCHER
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                SMOOTH_MENU::Simple_Menu* menu;
                LauncherRender_CB_t* menu_render_cb;
            };
        }

        class Launcher : public APP_BASE
        {
            private:
                const char* _tag = "launcher";

                LAUNCHER::Data_t _data;
                void _menu_init();
                void _icon_list_init();
                void _launcher_init();
                void _launcher_loop();
                void _app_open_callback(uint8_t selectedNum);
                void _simple_app_manager(MOONCAKE::APP_BASE* app);
```

to:

```cpp
        namespace LAUNCHER
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                SMOOTH_MENU::Simple_Menu* menu;
                LauncherRender_CB_t* menu_render_cb;

                bool screensaver_on = false;
                bool screensaver_initialized = false;
                uint32_t screensaver_last_activity_ms = 0;
                uint32_t screensaver_last_render_ms = 0;
                int64_t screensaver_last_encoder_count = 0;

                bool weather_ok = false;
                std::string weather_temp_c;
                std::string weather_condition;
            };
        }

        class Launcher : public APP_BASE
        {
            private:
                const char* _tag = "launcher";

                LAUNCHER::Data_t _data;
                void _menu_init();
                void _icon_list_init();
                void _launcher_init();
                void _launcher_loop();
                void _app_open_callback(uint8_t selectedNum);
                void _simple_app_manager(MOONCAKE::APP_BASE* app);

                void _screensaver_tick();
                void _screensaver_render();
                void _fetch_weather();
```

Also add this include near the top of `launcher.h`, alongside the existing app includes:

```cpp
#include "screensaver_config.h"
```

- [ ] **Step 2: Add the include and new methods to `launcher.cpp`**

Add near the top of `main/apps/launcher/launcher.cpp`, alongside the existing includes:

```cpp
#include "../utilities/weather_client/weather_client.h"
#include <ctime>
```

Append these new methods anywhere in the file (e.g. right after `_launcher_loop()`):

```cpp
void Launcher::_fetch_weather()
{
    WEATHER_CLIENT::WeatherInfo info = WEATHER_CLIENT::get_weather(WEATHER_API_BASE_URL);
    _data.weather_ok = info.ok;
    _data.weather_temp_c = info.temp_c;
    _data.weather_condition = info.condition;
}


void Launcher::_screensaver_render()
{
    struct tm time_now;
    _data.hal->rtc.getTime(time_now);

    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", time_now.tm_hour, time_now.tm_min);

    static const char* WEEKDAY_NAMES[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d %s",
             time_now.tm_year, time_now.tm_mon + 1, time_now.tm_mday,
             WEEKDAY_NAMES[time_now.tm_wday % 7]);

    _data.hal->canvas->fillScreen(TFT_BLACK);

    _data.hal->canvas->setTextColor(TFT_WHITE);
    _data.hal->canvas->setTextSize(4);
    int time_h = _data.hal->canvas->fontHeight();
    _data.hal->canvas->drawCenterString(time_buf, 120, 90 - time_h / 2);

    _data.hal->canvas->setTextSize(1);
    _data.hal->canvas->drawCenterString(date_buf, 120, 130);

    char weather_buf[32];
    if (_data.weather_ok)
    {
        snprintf(weather_buf, sizeof(weather_buf), "%s%cC %s",
                 _data.weather_temp_c.c_str(), 176 /* degree symbol */, _data.weather_condition.c_str());
    }
    else
    {
        snprintf(weather_buf, sizeof(weather_buf), "--");
    }
    _data.hal->canvas->drawCenterString(weather_buf, 120, 155);

    _data.hal->canvas->pushSprite(0, 0);
}


void Launcher::_screensaver_tick()
{
    if (!_data.screensaver_initialized)
    {
        _data.screensaver_last_activity_ms = millis();
        _data.screensaver_last_encoder_count = _data.hal->encoder.getCount();
        _data.screensaver_initialized = true;
    }

    bool touched = _data.hal->tp.isTouched();

    /* Read the raw count directly (no side effects) instead of
       wasMoved(), which the carousel's own scroll logic already
       consumes in _launcher_loop() - calling it here too would eat
       rotation before the carousel sees it. */
    int64_t current_count = _data.hal->encoder.getCount();
    bool encoder_moved = (current_count != _data.screensaver_last_encoder_count);
    _data.screensaver_last_encoder_count = current_count;

    bool button_pressed = !_data.hal->encoder.btn.read();

    bool activity = touched || encoder_moved || button_pressed;

    if (_data.screensaver_on)
    {
        if (activity)
        {
            /* Absorb the gesture that dismissed the screensaver, same
               "wake, don't act" convention as IDLE_SCREEN, so it can't
               also select whatever icon is under the touch. */
            if (touched)
            {
                while (_data.hal->tp.isTouched())
                {
                    _data.hal->tp.update();
                    delay(5);
                }
            }
            if (button_pressed)
            {
                while (!_data.hal->encoder.btn.read())
                    delay(5);
            }

            _data.screensaver_on = false;
            _data.screensaver_last_activity_ms = millis();
            return;
        }

        /* Refresh the displayed clock once a second while idle */
        if (millis() - _data.screensaver_last_render_ms >= 1000)
        {
            _screensaver_render();
            _data.screensaver_last_render_ms = millis();
        }
        return;
    }

    if (activity)
    {
        _data.screensaver_last_activity_ms = millis();
        return;
    }

    if (millis() - _data.screensaver_last_activity_ms > 30000)
    {
        _data.screensaver_on = true;
        _fetch_weather();
        _screensaver_render();
        _data.screensaver_last_render_ms = millis();
    }
}
```

- [ ] **Step 3: Wire `_screensaver_tick()` into `onRunning()`**

Change:

```cpp
void Launcher::onRunning()
{
    _launcher_loop();
}
```

to:

```cpp
void Launcher::onRunning()
{
    _screensaver_tick();
    if (!_data.screensaver_on)
    {
        _launcher_loop();
    }
}
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
git add main/apps/launcher/launcher.h main/apps/launcher/launcher.cpp
git commit -m "$(cat <<'EOF'
Add clock/date/weather screensaver to the Launcher

30s idle on the icon carousel shows a full-screen black/white clock,
date, and weather (fetched once per activation via WEATHER_CLIENT).
Any touch/encoder/button activity dismisses it, absorbing that one
gesture so it can't also select an icon - same convention as
IDLE_SCREEN, but this is a separate, independent 30s timer scoped
only to the Launcher (IDLE_SCREEN's own 5-minute backlight timeout
is unaffected and keeps running underneath). Activity is tracked via
encoder.getCount() directly, never wasMoved(), so it doesn't
interfere with the carousel's own scroll handling.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Wire `NTP_SYNC` into `main.cpp`

**Files:**
- Modify: `main/main.cpp`

**Interfaces:**
- Consumes: `NTP_SYNC::sync_rtc_time` (Task 1).

- [ ] **Step 1: Add the include**

```cpp
#include "apps/utilities/ntp_sync/ntp_sync.h"
```

- [ ] **Step 2: Call it after WiFi connects, before the Launcher starts**

Change:
```cpp
    WIFI_CONNECT::connect(RFID_WIFI_SSID, RFID_WIFI_PASSWORD, 8000);
    HA_CLIENT::init();
    RFID_SERVICE::init(&hal);
```
to:
```cpp
    WIFI_CONNECT::connect(RFID_WIFI_SSID, RFID_WIFI_PASSWORD, 8000);
    NTP_SYNC::sync_rtc_time(&hal, 8000);
    HA_CLIENT::init();
    RFID_SERVICE::init(&hal);
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
git add main/main.cpp
git commit -m "$(cat <<'EOF'
Sync RTC to internet time at boot via NTP_SYNC

Runs once, right after WiFi connects, before the launcher starts.
Best-effort - if it times out, the RTC just keeps whatever time it
already had.

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

- [ ] **Step 2: Flash**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py -p <PORT> flash
```

- [ ] **Step 3: Manual verification checklist**

- Boot the device fresh; check the serial log (or just trust the clock display) for `RTC synced: ...` at a plausible current date/time, not year 126 or similar garbage (confirms the `tm_year` conversion is correct).
- Sit on the Launcher carousel, untouched, for 30 seconds -> screensaver appears (black screen, time/date/weather).
- The displayed clock visibly ticks forward (minute changes after enough waiting, or watch the seconds indirectly via repeated checks).
- Touch the screen while the screensaver is showing -> it dismisses, returns to the carousel, and does NOT also open whatever icon was under the touch.
- Rotate the encoder while the screensaver is showing -> dismisses it, and does NOT also scroll the carousel to a different icon (the rotation that dismissed it shouldn't carry over).
- Press the encoder button while the screensaver is showing -> dismisses it, does not open the currently-selected icon.
- Open any app while the carousel is idle (before 30s) -> screensaver never appears while inside an app.
- After dismissing the screensaver, normal carousel scrolling/selection still works exactly as before.
- Leave the device alone for the full 5 minutes (through one or more screensaver cycles) -> `IDLE_SCREEN` still turns the backlight off on its own schedule, unaffected by the screensaver.
- Tap the phone RFID card at any point (carousel showing, screensaver showing) -> lights/fan still turn off (RFID_SERVICE regression check).
