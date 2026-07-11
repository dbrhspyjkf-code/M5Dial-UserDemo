# Sonos Control via Home Assistant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an M5Dial app that connects to WiFi, talks to Home Assistant's REST API, and controls a Sonos speaker (`media_player.ke_ting`) — play/pause, next/previous, volume via encoder, now-playing display.

**Architecture:** Two new dependency-free utilities (`wifi_connect_wrap`, `ha_client`) under `main/apps/utilities/`, consumed by a new `AppSonos`/`GUI_Sonos` pair under `main/apps/app_sonos/` following the exact file/naming pattern established by `app_timer`. Config (WiFi creds, HA token) is a gitignored header, not runtime-editable.

**Tech Stack:** ESP-IDF v5.1.3, `esp_http_client` (IDF built-in, needs `REQUIRES`), `cJSON` (IDF built-in component name `json`, needs `REQUIRES`), C++.

## Global Constraints

- No unit test harness in this codebase — "test" per task means `idf.py build` succeeds. Full behavior is verified once, at the end, by flashing and manually exercising every control against the real Sonos.
- Encoder button press-and-release means "quit the app", unconditionally, in every state — project-wide convention, same as `AppTimer` and every other app.
- Config (`sonos_config.h`) is compile-time, gitignored, never committed — see spec `docs/superpowers/specs/2026-07-11-sonos-ha-control-design.md`.
- Build environment: `source ~/esp-idf-v5.1.3/export.sh` before any `idf.py` command. Flash port: check `ls /dev/cu.*` each time — it has changed across sessions (`/dev/cu.usbmodem1101` and `/dev/cu.usbmodem112401` both seen).
- **The HA host/IP was never provided in chat** (only the token, entity ID, and WiFi credentials were). Task 3 below writes a placeholder for it — this must be filled in with the real HA host/IP before the final build/flash task can produce a working app.

---

### Task 1: WiFi connect/disconnect utility

**Files:**
- Create: `main/apps/utilities/wifi_connect_wrap/wifi_connect_wrap.h`
- Create: `main/apps/utilities/wifi_connect_wrap/wifi_connect_wrap.cpp`

**Interfaces:**
- Produces: `WIFI_CONNECT::connect(const char* ssid, const char* password, uint32_t timeout_ms) -> bool`, `WIFI_CONNECT::disconnect() -> void`. Consumed by Task 4 (`AppSonos`).

This refactors the connect pattern already working in `main/apps/utilities/wifi_common_test/wifi_factory_test.c` (`wifi_init_sta()`, event-group-based) and the teardown pattern already working in `main/apps/utilities/wifi_common_test/wifi_common_test.cpp` (`scan()`'s cleanup: `esp_wifi_stop` → `esp_wifi_deinit` → `esp_netif_destroy_default_wifi` → `esp_netif_deinit` → `esp_event_loop_delete_default` → `nvs_flash_deinit`) into one reusable, parameterized pair of functions instead of one-off hardcoded-SSID code.

- [ ] **Step 1: Write `main/apps/utilities/wifi_connect_wrap/wifi_connect_wrap.h`**

```cpp
/**
 * @file wifi_connect_wrap.h
 * @brief Reusable, blocking WiFi STA connect/disconnect. Refactored from
 * the working patterns already in wifi_factory_test.c (connect) and
 * wifi_common_test.cpp (teardown), parameterized instead of hardcoded.
 */
#pragma once
#include <cstdint>

namespace WIFI_CONNECT
{
    /**
     * @brief Connect to a WiFi AP, blocking until connected or timed out.
     *
     * @param ssid
     * @param password
     * @param timeout_ms
     * @return true if connected, false on failure/timeout
     */
    bool connect(const char* ssid, const char* password, uint32_t timeout_ms);

    /**
     * @brief Tear down WiFi fully (stop, deinit, destroy netif, delete
     * event loop, deinit NVS). Safe to call only after a successful connect().
     */
    void disconnect();
}
```

- [ ] **Step 2: Write `main/apps/utilities/wifi_connect_wrap/wifi_connect_wrap.cpp`**

```cpp
/**
 * @file wifi_connect_wrap.cpp
 */
#include "wifi_connect_wrap.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"


namespace WIFI_CONNECT
{
    static const char* TAG = "wifi_connect_wrap";

    static EventGroupHandle_t s_event_group = nullptr;
    static esp_netif_t* s_sta_netif = nullptr;
    static bool s_connected = false;

    #define WIFI_CONNECTED_BIT BIT0
    #define WIFI_FAIL_BIT       BIT1

    static void event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            xEventGroupSetBits(s_event_group, WIFI_FAIL_BIT);
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
        }
    }

    bool connect(const char* ssid, const char* password, uint32_t timeout_ms)
    {
        if (s_connected)
        {
            return true;
        }

        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        s_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_sta_netif = esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config));
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(
            s_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(timeout_ms));

        s_connected = (bits & WIFI_CONNECTED_BIT) != 0;

        if (!s_connected)
        {
            ESP_LOGE(TAG, "connect failed or timed out");
        }

        return s_connected;
    }

    void disconnect()
    {
        if (!s_connected)
        {
            return;
        }

        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(s_sta_netif);
        esp_netif_deinit();
        esp_event_loop_delete_default();
        nvs_flash_deinit();

        vEventGroupDelete(s_event_group);
        s_event_group = nullptr;
        s_sta_netif = nullptr;
        s_connected = false;
    }
}
```

- [ ] **Step 3: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.` (new files picked up automatically by the `apps/*.cpp` glob in `main/CMakeLists.txt`; nothing references these functions yet so this only checks they compile standalone).

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/utilities/wifi_connect_wrap
git commit -m "$(cat <<'EOF'
Add reusable WiFi connect/disconnect wrapper

Refactors the working connect pattern from wifi_factory_test.c and
the working teardown pattern from wifi_common_test.cpp into a
parameterized, reusable pair of functions instead of one-off
hardcoded-SSID code. Not yet used by any app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Home Assistant REST client

**Files:**
- Modify: `main/CMakeLists.txt`
- Create: `main/apps/utilities/ha_client/ha_client.h`
- Create: `main/apps/utilities/ha_client/ha_client.cpp`

**Interfaces:**
- Consumes: nothing from Task 1 (independent utility).
- Produces: `HA_CLIENT::MediaPlayerState` struct and `HA_CLIENT::get_state()` / `call_service()` / `set_volume()` functions, consumed by Task 4 (`AppSonos`).

- [ ] **Step 1: Add `esp_http_client` and `json` (cJSON) to the component's requirements**

Edit `main/CMakeLists.txt`, change the `idf_component_register` call at the bottom from:

```cmake
idf_component_register(SRCS "main.cpp" ${HAL_SRCS} ${APP_SRCS}
                    INCLUDE_DIRS "." ${HAL_INCS} ${APP_INCS})
```
to:
```cmake
idf_component_register(SRCS "main.cpp" ${HAL_SRCS} ${APP_SRCS}
                    INCLUDE_DIRS "." ${HAL_INCS} ${APP_INCS}
                    REQUIRES esp_http_client json)
```

- [ ] **Step 2: Write `main/apps/utilities/ha_client/ha_client.h`**

```cpp
/**
 * @file ha_client.h
 * @brief Thin wrapper around esp_http_client + cJSON for the two Home
 * Assistant REST call shapes this project needs: read a media_player's
 * state, and call a media_player service.
 */
#pragma once
#include <string>

namespace HA_CLIENT
{
    struct MediaPlayerState
    {
        bool ok = false;
        std::string state;   // e.g. "playing", "paused", "idle"
        std::string title;
        std::string artist;
        float volume = 0.0f; // 0.0 - 1.0
    };

    /**
     * @brief GET /api/states/{entity_id}
     */
    MediaPlayerState get_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/{domain}/{service} with body {"entity_id": entity_id}
     */
    bool call_service(const char* base_url, const char* token,
                       const char* domain, const char* service, const char* entity_id);

    /**
     * @brief POST /api/services/media_player/volume_set with
     * {"entity_id": entity_id, "volume_level": volume}
     */
    bool set_volume(const char* base_url, const char* token,
                     const char* entity_id, float volume);
}
```

- [ ] **Step 3: Write `main/apps/utilities/ha_client/ha_client.cpp`**

```cpp
/**
 * @file ha_client.cpp
 */
#include "ha_client.h"
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"


namespace HA_CLIENT
{
    static const char* TAG = "ha_client";

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

    static esp_http_client_handle_t _make_client(const char* url, const char* token,
                                                  esp_http_client_method_t method,
                                                  ResponseBuffer* resp_buf)
    {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = method;
        config.timeout_ms = 5000;
        config.event_handler = _http_event_handler;
        config.user_data = resp_buf;

        esp_http_client_handle_t client = esp_http_client_init(&config);

        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
        esp_http_client_set_header(client, "Authorization", auth_header);
        esp_http_client_set_header(client, "Content-Type", "application/json");

        return client;
    }

    MediaPlayerState get_state(const char* base_url, const char* token, const char* entity_id)
    {
        MediaPlayerState result;

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
            ESP_LOGE(TAG, "get_state failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.state = state->valuestring;
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* title = cJSON_GetObjectItem(attributes, "media_title");
            if (cJSON_IsString(title))
            {
                result.title = title->valuestring;
            }

            cJSON* artist = cJSON_GetObjectItem(attributes, "media_artist");
            if (cJSON_IsString(artist))
            {
                result.artist = artist->valuestring;
            }

            cJSON* volume = cJSON_GetObjectItem(attributes, "volume_level");
            if (cJSON_IsNumber(volume))
            {
                result.volume = (float)volume->valuedouble;
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }

    static bool _post_json(const char* base_url, const char* token,
                           const char* path, const char* json_body)
    {
        char url[256];
        snprintf(url, sizeof(url), "%s%s", base_url, path);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_handle_t client = _make_client(url, token, HTTP_METHOD_POST, &resp_buf);
        esp_http_client_set_post_field(client, json_body, (int)strlen(json_body));

        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "POST %s failed: err=%d status=%d", path, err, status);
            return false;
        }
        return true;
    }

    bool call_service(const char* base_url, const char* token,
                       const char* domain, const char* service, const char* entity_id)
    {
        char path[128];
        snprintf(path, sizeof(path), "/api/services/%s/%s", domain, service);

        char body[128];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\"}", entity_id);

        return _post_json(base_url, token, path, body);
    }

    bool set_volume(const char* base_url, const char* token,
                     const char* entity_id, float volume)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"volume_level\": %.2f}",
                 entity_id, volume);

        return _post_json(base_url, token, "/api/services/media_player/volume_set", body);
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
git add main/CMakeLists.txt main/apps/utilities/ha_client
git commit -m "$(cat <<'EOF'
Add Home Assistant REST client (get state, call service, set volume)

Wraps esp_http_client + cJSON. Adds esp_http_client and json (cJSON)
to main's component REQUIRES — first use of either in this codebase.
Not yet used by any app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Sonos config files

**Files:**
- Create: `main/apps/app_sonos/sonos_config.example.h` (committed template)
- Create: `main/apps/app_sonos/sonos_config.h` (gitignored, real values — **not committed by this task's git step**)

**Interfaces:**
- Produces: `SONOS_WIFI_SSID`, `SONOS_WIFI_PASSWORD`, `SONOS_HA_BASE_URL`, `SONOS_HA_TOKEN`, `SONOS_ENTITY_ID` macros, consumed by Task 4 (`AppSonos`).

`.gitignore` already has `main/apps/app_sonos/sonos_config.h` listed (added when the spec was written) — verify this before proceeding:

```bash
grep sonos_config ~/Projects/M5Dial-UserDemo/.gitignore
```
Expected: `main/apps/app_sonos/sonos_config.h`

- [ ] **Step 1: Create the directory and the committed example file**

```bash
mkdir -p ~/Projects/M5Dial-UserDemo/main/apps/app_sonos
```

Write `main/apps/app_sonos/sonos_config.example.h`:

```cpp
/**
 * @file sonos_config.example.h
 * @brief Copy this file to sonos_config.h (gitignored) and fill in real
 * values. sonos_config.h is never committed.
 */
#pragma once

#define SONOS_WIFI_SSID       "YOUR_WIFI_SSID"
#define SONOS_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define SONOS_HA_BASE_URL     "http://YOUR_HA_HOST:8123"
#define SONOS_HA_TOKEN        "YOUR_LONG_LIVED_ACCESS_TOKEN"
#define SONOS_ENTITY_ID       "media_player.your_entity"
```

- [ ] **Step 2: Create the real (gitignored) config file**

Write `main/apps/app_sonos/sonos_config.h` using the real WiFi SSID/password and HA long-lived token the user provided directly in chat earlier in this session (not reproduced in this plan document — this file must never be committed), the confirmed entity ID (`media_player.ke_ting`), and a placeholder for the HA host (not yet provided):

```cpp
/**
 * @file sonos_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define SONOS_WIFI_SSID       "<real SSID from chat>"
#define SONOS_WIFI_PASSWORD   "<real password from chat>"
#define SONOS_HA_BASE_URL     "http://REPLACE_WITH_HA_HOST:8123"
#define SONOS_HA_TOKEN        "<real long-lived token from chat>"
#define SONOS_ENTITY_ID       "media_player.ke_ting"
```

**Before Task 7 (build/flash), `SONOS_HA_BASE_URL` must be edited to the real HA host/IP** (e.g. `http://192.168.1.50:8123`) — it was never provided in chat.

- [ ] **Step 3: Build to verify the example header at least is syntactically fine**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.` (neither header is included by anything yet, so this just confirms nothing else broke).

- [ ] **Step 4: Commit — only the example file and .gitignore, never sonos_config.h**

```bash
cd ~/Projects/M5Dial-UserDemo
git status
```
Confirm `main/apps/app_sonos/sonos_config.h` shows as untracked but is **not** about to be added (check `.gitignore` is actually excluding it — it should not appear at all if `git status` output is clean of it, or appear under a section your git version labels as ignored).

```bash
git add main/apps/app_sonos/sonos_config.example.h
git commit -m "$(cat <<'EOF'
Add Sonos config template (sonos_config.example.h)

Real values live in the gitignored sonos_config.h, created locally
but never committed.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: AppSonos + GUI_Sonos

**Files:**
- Create: `main/apps/app_sonos/app_sonos.h`
- Create: `main/apps/app_sonos/app_sonos.cpp`
- Create: `main/apps/app_sonos/gui/gui_sonos.h`
- Create: `main/apps/app_sonos/gui/gui_sonos.cpp`

**Interfaces:**
- Consumes: `WIFI_CONNECT::connect/disconnect` (Task 1), `HA_CLIENT::MediaPlayerState/get_state/call_service/set_volume` (Task 2), `SONOS_*` macros (Task 3).
- Produces: `MOONCAKE::USER_APP::AppSonos` class, consumed by Task 6 (`launcher.h`/`launcher.cpp`).

- [ ] **Step 1: Write `main/apps/app_sonos/gui/gui_sonos.h`**

```cpp
/**
 * @file gui_sonos.h
 * @brief Renderer for the Sonos app. Pure display — takes primitive
 * values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_Sonos : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the connecting/error screen (no track info yet)
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render the normal playing/paused screen
         *
         * @param title track title (may be empty)
         * @param artist track artist (may be empty)
         * @param isPlaying true if state == "playing" (affects play/pause glyph)
         * @param volumePercent 0-100
         */
        void renderPlaying(const std::string& title, const std::string& artist,
                            bool isPlaying, int volumePercent);
};
```

- [ ] **Step 2: Write `main/apps/app_sonos/gui/gui_sonos.cpp`**

```cpp
/**
 * @file gui_sonos.cpp
 */
#include "gui_sonos.h"
#include <cstdio>


void GUI_Sonos::init()
{
}


void GUI_Sonos::renderStatus(const std::string& line1, const std::string& line2)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("SONOS", 120, 45);

    _canvas->setTextSize(2);
    int h1 = _canvas->fontHeight();
    _canvas->drawCenterString(line1.c_str(), 120, 110 - h1 / 2);

    _canvas->setTextSize(1);
    _canvas->drawCenterString(line2.c_str(), 120, 140);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}


void GUI_Sonos::renderPlaying(const std::string& title, const std::string& artist,
                               bool isPlaying, int volumePercent)
{
    _canvas->fillScreen(_theme_color);

    _draw_top_icon();

    _canvas->setTextColor(TFT_WHITE);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("SONOS", 120, 40);

    /* Title, truncated with "..." if it doesn't fit in ~200px */
    _canvas->setTextSize(2);
    std::string display_title = title.empty() ? "(nothing playing)" : title;
    while (_canvas->textWidth(display_title.c_str()) > 200 && display_title.size() > 3)
    {
        display_title = display_title.substr(0, display_title.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_title.c_str(), 120, 62);

    /* Artist, dimmer, smaller */
    _canvas->setTextColor(0x999999U);
    _canvas->setTextSize(1);
    std::string display_artist = artist;
    while (_canvas->textWidth(display_artist.c_str()) > 200 && display_artist.size() > 3)
    {
        display_artist = display_artist.substr(0, display_artist.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_artist.c_str(), 120, 88);

    /* Volume readout */
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "VOL %d%%", volumePercent);
    _canvas->setTextColor(TFT_WHITE);
    _canvas->drawCenterString(vol_buf, 120, 108);

    /* Prev / Play-Pause / Next buttons */
    struct BtnSpec { int x; const char* label; };
    BtnSpec buttons[3] = {
        {55,  "<<"},
        {120, isPlaying ? "||" : ">"},
        {185, ">>"},
    };

    int btn_y = 145;
    int btn_height = 32;

    for (int i = 0; i < 3; i++)
    {
        _canvas->setTextSize(1);
        int text_w = _canvas->textWidth(buttons[i].label);
        int text_h = _canvas->fontHeight();
        int btn_width = text_w + 30;
        if (btn_width < 44) btn_width = 44;

        _canvas->fillSmoothRoundRect(buttons[i].x - btn_width / 2, btn_y - btn_height / 2,
                                      btn_width, btn_height, btn_height / 2, TFT_WHITE);
        _canvas->setTextColor(TFT_BLACK);
        _canvas->drawCenterString(buttons[i].label, buttons[i].x, btn_y - text_h / 2);
    }

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
```

- [ ] **Step 3: Write `main/apps/app_sonos/app_sonos.h`**

```cpp
/**
 * @file app_sonos.h
 * @brief Controls a Sonos speaker through Home Assistant's REST API.
 * Touch: prev/play-pause/next. Encoder: volume (debounced). Encoder
 * button: quit (project-wide convention, same as every other app).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_sonos.h"
#include "sonos_config.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace SONOS
        {
            enum class State { CONNECTING, POLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                std::string title;
                std::string artist;
                bool is_playing = false;
                float volume = 0.5f;

                uint32_t last_poll_ms = 0;
                int poll_fail_count = 0;

                /* Encoder volume debounce */
                bool volume_dirty = false;
                uint32_t last_volume_change_ms = 0;
            };
        }

        class AppSonos : public APP_BASE
        {
            private:
                const char* _tag = "AppSonos";

                void _handle_encoder();
                void _handle_touch();
                void _handle_poll();
                void _handle_volume_debounce();
                void _render();
                void _refresh_state();

            public:
                SONOS::Data_t _data;
                GUI_Sonos _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
```

- [ ] **Step 4: Write `main/apps/app_sonos/app_sonos.cpp`**

```cpp
/**
 * @file app_sonos.cpp
 */
#include "app_sonos.h"
#include "../common_define.h"
#include "../utilities/wifi_connect_wrap/wifi_connect_wrap.h"
#include "../utilities/ha_client/ha_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::SONOS;


void AppSonos::onSetup()
{
    setAppName("AppSonos");
    setAllowBgRunning(false);

    SONOS::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void AppSonos::_refresh_state()
{
    HA_CLIENT::MediaPlayerState mp = HA_CLIENT::get_state(
        SONOS_HA_BASE_URL, SONOS_HA_TOKEN, SONOS_ENTITY_ID);

    if (!mp.ok)
    {
        _data.poll_fail_count++;
        if (_data.poll_fail_count >= 3)
        {
            _data.state = State::ERROR;
            _data.error_message = "HA unreachable";
        }
        return;
    }

    _data.poll_fail_count = 0;
    _data.title = mp.title;
    _data.artist = mp.artist;
    _data.is_playing = (mp.state == "playing");
    if (!_data.volume_dirty)
    {
        _data.volume = mp.volume;
    }
}


void AppSonos::onCreate()
{
    _log("onCreate");
    _data.state = State::CONNECTING;
    _render();

    bool connected = WIFI_CONNECT::connect(SONOS_WIFI_SSID, SONOS_WIFI_PASSWORD, 8000);
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
        _data.state = State::POLLING;
    }
    _data.last_poll_ms = millis();
    _render();
}


void AppSonos::_handle_encoder()
{
    if (_data.state != State::POLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    float step = (_data.hal->encoder.getDirection() < 1) ? 0.05f : -0.05f;
    _data.volume += step;
    if (_data.volume < 0.0f) _data.volume = 0.0f;
    if (_data.volume > 1.0f) _data.volume = 1.0f;

    _data.volume_dirty = true;
    _data.last_volume_change_ms = millis();

    _render();
}


void AppSonos::_handle_volume_debounce()
{
    if (!_data.volume_dirty)
        return;

    if (millis() - _data.last_volume_change_ms < 400)
        return;

    HA_CLIENT::set_volume(SONOS_HA_BASE_URL, SONOS_HA_TOKEN, SONOS_ENTITY_ID, _data.volume);
    _data.volume_dirty = false;
}


void AppSonos::_handle_touch()
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
    else if (_data.state == State::POLLING && y >= 129 && y <= 161)
    {
        if (x >= 30 && x <= 80)
        {
            HA_CLIENT::call_service(SONOS_HA_BASE_URL, SONOS_HA_TOKEN,
                                     "media_player", "media_previous_track", SONOS_ENTITY_ID);
        }
        else if (x >= 95 && x <= 145)
        {
            HA_CLIENT::call_service(SONOS_HA_BASE_URL, SONOS_HA_TOKEN,
                                     "media_player", "media_play_pause", SONOS_ENTITY_ID);
        }
        else if (x >= 160 && x <= 210)
        {
            HA_CLIENT::call_service(SONOS_HA_BASE_URL, SONOS_HA_TOKEN,
                                     "media_player", "media_next_track", SONOS_ENTITY_ID);
        }
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void AppSonos::_handle_poll()
{
    if (_data.state != State::POLLING)
        return;

    if (millis() - _data.last_poll_ms < 3000)
        return;

    _data.last_poll_ms = millis();
    _refresh_state();
    _render();
}


void AppSonos::_render()
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
        int volume_percent = (int)(_data.volume * 100.0f + 0.5f);
        _gui.renderPlaying(_data.title, _data.artist, _data.is_playing, volume_percent);
    }
}


void AppSonos::onRunning()
{
    _handle_encoder();
    _handle_volume_debounce();
    _handle_touch();
    _handle_poll();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


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
git add main/apps/app_sonos/app_sonos.h main/apps/app_sonos/app_sonos.cpp main/apps/app_sonos/gui
git commit -m "$(cat <<'EOF'
Add AppSonos: Sonos control via Home Assistant REST API

Connect/poll/error state machine on top of wifi_connect_wrap and
ha_client. Touch: prev/play-pause/next. Encoder: debounced volume.
Encoder button: quit (matches every other app). Not yet wired into
the launcher.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Sonos launcher icon

**Files:**
- Create: `main/apps/launcher/launcher_icons/icon_sonos.h`
- Modify: `main/apps/launcher/launcher_icons/launcher_icons.h`

**Interfaces:**
- Produces: `static const uint16_t image_data_icon_sonos[1764]`, consumed by Task 6's `icon_pic_list`.

This reuses the exact recolor → resize-to-42×42 → hard-circular-mask → byte-swapped-RGB565 pipeline already worked out (through several iterations) for the Timer icon. It needs a source image the same shape as `timer.png` was: a flat white-line-art glyph on a solid background, at any resolution, at `~/Desktop/sonos_icon.png` (or wherever the user saves it) — **ask the user for this image before starting this task** (same as was done for the Timer icon), the way `timer.png` was requested. Pick a new accent color not already used: existing colors are `0xFD5C4C, 0x577EFF, 0x03A964, 0x1AA198, 0xEB8429, 0x04A279, 0x008CD6, 0x5D7BA2, 0xB565F3` — use `0xFF6FAE` (pink) for Sonos.

- [ ] **Step 1: Get the source image path from the user, then generate the icon**

```bash
cd ~/Projects/M5Dial-UserDemo/main/apps/launcher/launcher_icons
python3 - <<'EOF'
from PIL import Image

src = Image.open('/Users/leenzhou/Desktop/sonos_icon.png').convert('RGB')  # replace with actual path
w, h = src.size

NEW_BG = (0xFF, 0x6F, 0xAE)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
BG_G = 128     # adjust if the source image's flat background isn't (194,128,255)-style;
WHITE_G = 255  # sample src.getpixel((2,2)) first to confirm the background color's green channel

px = src.load()
out = Image.new('RGB', (w, h))
outpx = out.load()

for y in range(h):
    for x in range(w):
        r, g, b = px[x, y]
        t = (g - BG_G) / (WHITE_G - BG_G)
        t = max(0.0, min(1.0, t))
        nr = int(NEW_BG[0] * (1 - t) + WHITE[0] * t)
        ng = int(NEW_BG[1] * (1 - t) + WHITE[1] * t)
        nb = int(NEW_BG[2] * (1 - t) + WHITE[2] * t)
        outpx[x, y] = (nr, ng, nb)

SIZE = 42
small = out.resize((SIZE, SIZE), Image.LANCZOS)
smallpx = small.load()

cx = cy = SIZE / 2
r_mask = SIZE * 0.46
for y in range(SIZE):
    for x in range(SIZE):
        dx = x - cx + 0.5
        dy = y - cy + 0.5
        if dx * dx + dy * dy > r_mask * r_mask:
            smallpx[x, y] = BLACK

def to565_swapped(r, g, b):
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return ((v & 0xFF) << 8) | ((v >> 8) & 0xFF)

values = [to565_swapped(r, g, b) for (r, g, b) in small.getdata()]

with open("icon_sonos.h", "w") as f:
    f.write("#pragma once\n#include <stdint.h>\n\n\n")
    f.write(f"static const uint16_t image_data_icon_sonos[{len(values)}] = {{\n")
    for i in range(0, len(values), 16):
        row = values[i:i + 16]
        f.write("    " + ", ".join(f"0x{v:04x}" for v in row) + ",\n")
    f.write("};\n")

small.save("icon_sonos_preview.png")
print("wrote", len(values), "values")
EOF
```
Expected output: `wrote 1764 values`. View `icon_sonos_preview.png` to confirm it looks like a clean pink circle with a white glyph, no jagged/garbled colors (if colors look wrong, the `BG_G`/background-color assumption needs adjusting to match this specific source image — check `src.getpixel((2,2))` first).

- [ ] **Step 2: Register the icon**

Edit `main/apps/launcher/launcher_icons/launcher_icons.h`, add (alphabetical order):

```cpp
#include "icon_rtc.h"
#include "icon_sonos.h"
#include "icon_temp.h"
#include "icon_timer.h"
#include "icon_wifi.h"
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
git add main/apps/launcher/launcher_icons/icon_sonos.h main/apps/launcher/launcher_icons/icon_sonos_preview.png main/apps/launcher/launcher_icons/launcher_icons.h
git commit -m "$(cat <<'EOF'
Add Sonos launcher icon (0xFF6FAE accent)

Same recolor + hard-circular-mask + byte-swapped-RGB565 pipeline
worked out for the Timer icon.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Wire AppSonos into the launcher (10th icon)

**Files:**
- Modify: `main/apps/launcher/launcher_render_callback.hpp`
- Modify: `main/apps/launcher/launcher.h`
- Modify: `main/apps/launcher/launcher.cpp`

**Interfaces:**
- Consumes: `MOONCAKE::USER_APP::AppSonos` (Task 4), `image_data_icon_sonos` (Task 5).
- Produces: end-to-end working 10th launcher icon that opens the Sonos app. This is the **last** icon slot — the circular menu grid has exactly 10 positions (`n = 10` in `launcher.cpp::_menu_init()`), already confirmed when the Timer icon filled slot 9 of 10.

- [ ] **Step 1: Bump `ICON_NUM` and extend the icon arrays**

In `main/apps/launcher/launcher_render_callback.hpp`, change:
```cpp
#define ICON_NUM                    9
```
to:
```cpp
#define ICON_NUM                    10
```

Extend `icon_color_list`:
```cpp
static std::array<uint32_t, ICON_NUM> icon_color_list = {
    0xFD5C4C,
    0x577EFF,
    0x03A964,
    0x1AA198,
    0xEB8429,
    0x04A279,
    0x008CD6,
    0x5D7BA2,
    0xB565F3,
    0xFF6FAE
};
```

Extend `icon_tag_list`:
```cpp
static std::array<std::string, ICON_NUM * 2> icon_tag_list = {
    "LCD", "TEST",
    "RTC", "TIME",
    "RFID", "TEST",
    "BRIGHTNESS", "SET",
    "WIFI", "SCAN",
    "BLE", "SERVER",
    "TEMP CTRL", "DEMO",
    "MORE", "",
    "TIMER", "SET",
    "SONOS", "CTRL"
};
```

Extend `icon_pic_list`:
```cpp
static std::array<const uint16_t*, ICON_NUM> icon_pic_list = {
    image_data_icon_lcd,
    image_data_icon_rtc,
    image_data_icon_rfid,
    image_data_icon_brigntness,
    image_data_icon_wifi,
    image_data_icon_ble,
    image_data_icon_temp,
    image_data_icon_more,
    image_data_icon_timer,
    image_data_icon_sonos
};
```

- [ ] **Step 2: Include the new app header in `launcher.h`**

```cpp
#include "../app_timer/app_timer.h"
#include "../app_sonos/app_sonos.h"
```

- [ ] **Step 3: Add the switch case in `launcher.cpp`**

In `Launcher::_app_open_callback`:
```cpp
        case 8:
            app_ptr = new MOONCAKE::USER_APP::AppTimer;
            break;
        case 9:
            app_ptr = new MOONCAKE::USER_APP::AppSonos;
            break;
        default:
            break;
    };
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
git add main/apps/launcher/launcher_render_callback.hpp main/apps/launcher/launcher.h main/apps/launcher/launcher.cpp
git commit -m "$(cat <<'EOF'
Wire Sonos app into the launcher as the 10th (last) icon

Bumps ICON_NUM 9->10, filling the final slot in the circular menu's
10-position grid.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Flash and manually verify

**Files:** none (verification only)

- [ ] **Step 1: Confirm `SONOS_HA_BASE_URL` has been filled in with the real HA host/IP**

```bash
grep SONOS_HA_BASE_URL ~/Projects/M5Dial-UserDemo/main/apps/app_sonos/sonos_config.h
```
If it still shows `REPLACE_WITH_HA_HOST`, stop and get the real host/IP from the user before continuing — the app cannot reach Home Assistant otherwise.

- [ ] **Step 2: Flash the device**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
ls /dev/cu.*   # confirm the port hasn't changed
idf.py -p <PORT> flash
```
Expected: flashing completes, `Hash of data verified.` for both writes, device hard-resets.

- [ ] **Step 3: Manual verification checklist**

On the device, open the 10th icon (pink, tagged "SONOS"/"CTRL"):
- [ ] Shows "Connecting..." briefly, then either the now-playing screen or an error screen
- [ ] If Sonos is playing something: title and artist are shown (truncated with "..." if long), volume percentage shown
- [ ] Tap play/pause → Sonos actually pauses/resumes within ~1 second
- [ ] Tap next → Sonos skips to the next track; tap prev → goes to previous track
- [ ] Rotate the encoder → Sonos volume actually changes (audible), and rotating it several times quickly only sends one HA request after you stop (not one per detent — check HA's logbook doesn't show a flood of volume_set calls)
- [ ] Wait ~3+ seconds after changing track externally (e.g. from the Sonos app on your phone) → the M5Dial screen updates to match within one poll cycle
- [ ] Turn off WiFi (disable the AP, or take the M5Dial out of range) and reopen the app → shows an error screen, not a hang; tapping the error screen retries
- [ ] Encoder button press-and-release from the connecting, playing, and error screens all return to the launcher

- [ ] **Step 4: Fix any issues found, rebuild, reflash, recheck**

Same iterative loop as the Timer app's final task — adjust touch-zone coordinates in `app_sonos.cpp`, layout constants in `gui_sonos.cpp`, or HA request logic in `ha_client.cpp` as needed, based on what's actually observed on the device. Commit fixes:

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_sonos
git commit -m "$(cat <<'EOF'
Adjust Sonos app after on-device testing

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Skip this commit if nothing needed adjusting.)

---

## Self-Review

**Spec coverage:**
- WiFi connect wrapper (reusable, not hardcoded) — Task 1. ✓
- HA REST API over HTTP + long-lived token — Task 2 (`Authorization: Bearer` header). ✓
- Compile-time config, gitignored — Task 3. ✓
- Play/pause, next/previous, volume via encoder, now-playing display — Task 4 (`_handle_touch`, `_handle_encoder`, `renderPlaying`). ✓
- 3-second poll — Task 4 (`_handle_poll`, `millis() - _data.last_poll_ms < 3000`). ✓
- WiFi connected only while app is open — Task 4 (`onCreate` connects, `onDestroy` disconnects). ✓
- ERROR state with retry — Task 4 (`_handle_touch`'s `State::ERROR` branch calls `onCreate()` again). ✓
- New icon, same pipeline as Timer — Task 5. ✓
- Launcher integration, 10th/last slot — Task 6. ✓
- Testing/verification plan — Task 7. ✓
- Secrets never committed — Task 3 explicitly checks `git status` before adding, `.gitignore` entry verified first.

**Placeholder scan:** no TBD/TODO; all code blocks are complete, runnable code. The one deliberate placeholder (`REPLACE_WITH_HA_HOST` inside `sonos_config.h`) is a real, flagged, actionable gap — not a plan-writing shortcut — and Task 7 Step 1 explicitly checks for it before flashing.

**Type consistency:** `HA_CLIENT::MediaPlayerState` fields (`ok`, `state`, `title`, `artist`, `volume`) match between Task 2's definition and Task 4's usage (`mp.ok`, `mp.title`, etc.). `SONOS::State` enum (`CONNECTING`/`POLLING`/`ERROR`) used consistently through Task 4. `GUI_Sonos::renderStatus`/`renderPlaying` signatures match between Task 4's header declaration, Task 4's `.cpp` definition, and Task 4's call sites in `_render()`.
