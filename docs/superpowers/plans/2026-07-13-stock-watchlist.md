# Stock Watchlist Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repurpose `app_more_menu` (`MoreMenu`, currently a placeholder demo menu) into a read-only stock watchlist viewer, pulling live quotes from `http://192.168.1.200:8766/api/stocks/portfolio`.

**Architecture:** New `STOCK_CLIENT::get_portfolio()` does a one-shot HTTP GET (no persistent connection, no auth — see rationale in the design spec). `MoreMenu` gains a proper `GUI_Base`-derived GUI (`GUI_MoreMenu`, new — the old placeholder never had one) and a `CONNECTING`/`CONTROLLING`/`ERROR`-style state machine like the other repurposed apps, except "CONTROLLING" here just means "showing the fetched list", with no writes back to any server. Class name (`MoreMenu`) and launcher wiring (`case 7`) stay unchanged.

**Tech Stack:** ESP-IDF v5.1.3, esp_http_client + cJSON, LovyanGFX (`GUI_Base`).

## Global Constraints

- WiFi is already connected at boot (`main.cpp`) — no `WIFI_CONNECT::connect()` call needed in this app at all (unlike the 4 HA apps, this endpoint needs no auth and WiFi is already up device-wide; calling `connect()` again would just be a redundant no-op, so it's simplest to skip it entirely here).
- Encoder button press-and-release = unconditional quit (project-wide convention).
- Fetch happens exactly once per app open, in `onCreate()` — no periodic refresh.
- Encoder rotation wraps around the list (past the last item goes to the first, and vice versa).
- Colors: red = positive change, green = negative change (A-share convention, opposite of the US convention).

---

### Task 1: Create the `STOCK_CLIENT` utility

**Files:**
- Create: `main/apps/utilities/stock_client/stock_client.h`
- Create: `main/apps/utilities/stock_client/stock_client.cpp`

**Interfaces:**
- Produces: `STOCK_CLIENT::StockItem{code, name, price, chg}`, `STOCK_CLIENT::get_portfolio(const char* base_url) -> std::vector<StockItem>`. Consumed by Task 3 (`app_more_menu.cpp`).

- [ ] **Step 1: Write `stock_client.h`**

```cpp
/**
 * @file stock_client.h
 * @brief One-shot HTTP client for the user's hermes-mcp-xiaozhi stock
 * watchlist API. No persistent connection (unlike HA_CLIENT) - this
 * endpoint is only ever called once per app open, not continuously
 * polled, and its response body (~12KB, includes analysis text this
 * app doesn't even display) is far larger than HA_CLIENT's fixed
 * 2048-byte buffer, so a fresh malloc'd buffer per call is simpler
 * and correct at this call frequency.
 */
#pragma once
#include <string>
#include <vector>

namespace STOCK_CLIENT
{
    struct StockItem
    {
        std::string code;
        std::string name;
        float price = 0.0f;
        float chg = 0.0f;  // percentage change, e.g. 5.34 means +5.34%
    };

    /**
     * @brief GET {base_url}/api/stocks/portfolio, parse the "items"
     * array. Returns an empty vector on any failure (unreachable,
     * non-200, JSON parse failure, malloc failure).
     */
    std::vector<StockItem> get_portfolio(const char* base_url);
}
```

- [ ] **Step 2: Write `stock_client.cpp`**

```cpp
/**
 * @file stock_client.cpp
 */
#include "stock_client.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace STOCK_CLIENT
{
    static const char* TAG = "stock_client";

    struct ResponseBuffer
    {
        char* data;
        int len;
        int capacity;
    };

    static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            auto* buf = (ResponseBuffer*)evt->user_data;
            int space = buf->capacity - buf->len - 1;
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

    std::vector<StockItem> get_portfolio(const char* base_url)
    {
        std::vector<StockItem> result;

        char url[128];
        snprintf(url, sizeof(url), "%s/api/stocks/portfolio", base_url);

        const int BUF_CAPACITY = 16384;
        ResponseBuffer resp_buf;
        resp_buf.data = (char*)malloc(BUF_CAPACITY);
        resp_buf.len = 0;
        resp_buf.capacity = BUF_CAPACITY;
        if (resp_buf.data == nullptr)
        {
            ESP_LOGE(TAG, "get_portfolio: malloc failed");
            return result;
        }
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
            ESP_LOGE(TAG, "get_portfolio failed: err=%d status=%d", err, status);
            free(resp_buf.data);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        free(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_portfolio: JSON parse failed");
            return result;
        }

        cJSON* items = cJSON_GetObjectItem(root, "items");
        if (cJSON_IsArray(items))
        {
            int count = cJSON_GetArraySize(items);
            for (int i = 0; i < count; i++)
            {
                cJSON* item = cJSON_GetArrayItem(items, i);
                if (item == nullptr) continue;

                StockItem stock;

                cJSON* code = cJSON_GetObjectItem(item, "code");
                if (cJSON_IsString(code)) stock.code = code->valuestring;

                cJSON* name = cJSON_GetObjectItem(item, "name");
                if (cJSON_IsString(name)) stock.name = name->valuestring;

                cJSON* price = cJSON_GetObjectItem(item, "price");
                if (cJSON_IsNumber(price)) stock.price = (float)price->valuedouble;

                cJSON* chg = cJSON_GetObjectItem(item, "chg");
                if (cJSON_IsNumber(chg)) stock.chg = (float)chg->valuedouble;

                result.push_back(stock);
            }
        }

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
git add main/apps/utilities/stock_client
git commit -m "$(cat <<'EOF'
Add STOCK_CLIENT: one-shot fetch for the stock watchlist API

GETs {base_url}/api/stocks/portfolio into a malloc'd 16KB buffer
(response is ~12KB, confirmed via live curl - far past HA_CLIENT's
fixed 2048-byte buffer) and parses code/name/price/chg per stock.
No persistent connection - this is called once per app open, not
continuously polled, so HA_CLIENT's leak concern doesn't apply.
Not yet used - no caller until the stock watchlist app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Config file

**Files:**
- Create: `main/apps/app_more_menu/stock_config.example.h`
- Create: `main/apps/app_more_menu/stock_config.h` (gitignored)
- Modify: `.gitignore`

**Interfaces:**
- Produces: `STOCK_API_BASE_URL` macro. Consumed by Task 3.

- [ ] **Step 1: Add the gitignore entry**

Edit `.gitignore`, add a line after the existing `main/apps/app_temp_demo/ac_config.h` entry:

```
main/apps/app_more_menu/stock_config.h
```

- [ ] **Step 2: Write the committed example config**

Write `main/apps/app_more_menu/stock_config.example.h`:

```cpp
/**
 * @file stock_config.example.h
 * @brief Copy this file to stock_config.h (gitignored) and fill in
 * real values. stock_config.h is never committed.
 */
#pragma once

#define STOCK_API_BASE_URL   "http://YOUR_HOST:8766"
```

- [ ] **Step 3: Write the real (gitignored) config**

Write `main/apps/app_more_menu/stock_config.h`:

```cpp
/**
 * @file stock_config.h
 * @brief Real values — gitignored, never committed.
 */
#pragma once

#define STOCK_API_BASE_URL   "http://192.168.1.200:8766"
```

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add .gitignore main/apps/app_more_menu/stock_config.example.h
git commit -m "$(cat <<'EOF'
Add stock_config template for the stock watchlist app

Real stock_config.h is gitignored, not part of this commit.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

(`stock_config.h` itself is gitignored and won't show up as trackable — nothing more to commit for it.)

---

### Task 3: Replace `app_more_menu.h` / `.cpp` with the stock watchlist viewer

**Files:**
- Modify: `main/apps/app_more_menu/app_more_menu.h`
- Modify: `main/apps/app_more_menu/app_more_menu.cpp`
- Delete: `main/apps/app_more_menu/more_menu_render_callback.hpp` (the old `SMOOTH_MENU` render callback — no longer used)

**Interfaces:**
- Consumes: `STOCK_CLIENT::get_portfolio` (Task 1), `STOCK_API_BASE_URL` (Task 2).
- Produces: calls into `GUI_MoreMenu::renderStatus(line1, line2)` and `GUI_MoreMenu::renderPage(const std::string& name, float price, float chg, int index, int count)` — signatures defined in Task 4.

- [ ] **Step 1: Replace `main/apps/app_more_menu/app_more_menu.h`**

```cpp
/**
 * @file app_more_menu.h
 * @brief Read-only viewer for the user's stock watchlist, fetched
 * once per app open from the hermes-mcp-xiaozhi stock API. Encoder
 * rotates through the list (wraps around). No writes to any server -
 * this app is display-only.
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_more_menu.h"
#include "stock_config.h"
#include <vector>


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace MORE_MENU
        {
            enum class State { CONNECTING, CONTROLLING, ERROR };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                std::vector<STOCK_CLIENT::StockItem> stocks;
                int current_index = 0;
            };
        } // namespace MORE_MENU

        class MoreMenu : public APP_BASE
        {
            private:
                const char* _tag = "stock";

                void _handle_encoder();
                void _handle_touch();
                void _render();
                void _fetch();

            public:
                MORE_MENU::Data_t _data;
                GUI_MoreMenu _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
```

- [ ] **Step 2: Replace `main/apps/app_more_menu/app_more_menu.cpp`**

```cpp
/**
 * @file app_more_menu.cpp
 */
#include "app_more_menu.h"
#include "../common_define.h"
#include "../utilities/stock_client/stock_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::MORE_MENU;


void MoreMenu::onSetup()
{
    setAppName("MoreMenu");
    setAllowBgRunning(false);

    MORE_MENU::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void MoreMenu::_fetch()
{
    _data.stocks = STOCK_CLIENT::get_portfolio(STOCK_API_BASE_URL);

    if (_data.stocks.empty())
    {
        _data.state = State::ERROR;
        _data.error_message = "No data";
        return;
    }

    if (_data.current_index >= (int)_data.stocks.size())
    {
        _data.current_index = 0;
    }
    _data.state = State::CONTROLLING;
}


void MoreMenu::onCreate()
{
    _log("onCreate");

    _data.state = State::CONNECTING;
    _render();

    _fetch();
    _render();
}


void MoreMenu::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;
    int count = (int)_data.stocks.size();

    _data.current_index = (_data.current_index + direction + count) % count;

    _render();
}


void MoreMenu::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    if (_data.state == State::ERROR)
    {
        _data.state = State::CONNECTING;
        _render();
        _fetch();
        _render();
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void MoreMenu::_render()
{
    if (_data.state == State::CONNECTING)
    {
        _gui.renderStatus("Loading...", "");
    }
    else if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        const STOCK_CLIENT::StockItem& stock = _data.stocks[_data.current_index];
        std::string display_name = stock.name.empty() ? stock.code : stock.name;
        _gui.renderPage(display_name, stock.price, stock.chg,
                         _data.current_index, (int)_data.stocks.size());
    }
}


void MoreMenu::onRunning()
{
    _handle_encoder();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void MoreMenu::onDestroy()
{
    _log("onDestroy");

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
```

- [ ] **Step 3: Delete the old menu render callback**

```bash
rm ~/Projects/M5Dial-UserDemo/main/apps/app_more_menu/more_menu_render_callback.hpp
```

- [ ] **Step 4: Build to verify it compiles**

This will fail until Task 4 adds `gui/gui_more_menu.h`/`.cpp` - build after Task 4 instead. Skip building at the end of this task; proceed directly to Task 4.

---

### Task 4: Create `gui/gui_more_menu.h` / `.cpp`

**Files:**
- Create: `main/apps/app_more_menu/gui/gui_more_menu.h`
- Create: `main/apps/app_more_menu/gui/gui_more_menu.cpp`

**Interfaces:**
- Consumes: nothing new (pure rendering, same `GUI_Base` helpers `_draw_top_icon()`/`_draw_quit_button()`/`_theme_color`/`_canvas` used by every other app's GUI).
- Produces: `GUI_MoreMenu::renderStatus(const std::string& line1, const std::string& line2)`, `GUI_MoreMenu::renderPage(const std::string& name, float price, float chg, int index, int count)` — consumed by Task 3.

- [ ] **Step 1: Write `main/apps/app_more_menu/gui/gui_more_menu.h`**

```cpp
/**
 * @file gui_more_menu.h
 * @brief Renderer for the stock watchlist app. Pure display — takes
 * primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_MoreMenu : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the loading/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render one stock
         *
         * @param name stock name (or code, if name is empty)
         * @param price current price
         * @param chg percentage change (positive = up, negative = down)
         * @param index 0-based position in the list
         * @param count total number of stocks
         */
        void renderPage(const std::string& name, float price, float chg, int index, int count);
};
```

- [ ] **Step 2: Write `main/apps/app_more_menu/gui/gui_more_menu.cpp`**

```cpp
/**
 * @file gui_more_menu.cpp
 */
#include "gui_more_menu.h"
#include <cstdio>


void GUI_MoreMenu::init()
{
}


void GUI_MoreMenu::renderStatus(const std::string& line1, const std::string& line2)
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


void GUI_MoreMenu::renderPage(const std::string& name, float price, float chg, int index, int count)
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

    /* Stock name, truncated with "..." if it doesn't fit */
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    std::string display_name = name;
    while (_canvas->textWidth(display_name.c_str()) > 200 && display_name.size() > 3)
    {
        display_name = display_name.substr(0, display_name.size() - 4) + "...";
    }
    _canvas->drawCenterString(display_name.c_str(), bubble.x, bubble.y - 48);

    /* Price, large */
    char price_buffer[24];
    snprintf(price_buffer, sizeof(price_buffer), "%.2f", price);
    _canvas->setTextSize(3);
    _canvas->drawCenterString(price_buffer, bubble.x, bubble.y - 26);

    /* Change %, colored - red for up, green for down (A-share convention) */
    char chg_buffer[24];
    snprintf(chg_buffer, sizeof(chg_buffer), "%+.2f%%", chg);
    uint32_t chg_color = (chg >= 0) ? TFT_RED : 0x00B050U;
    _canvas->setTextColor(chg_color);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(chg_buffer, bubble.x, bubble.y + 20);

    /* Position indicator, e.g. "3 / 11" */
    char position_buffer[16];
    snprintf(position_buffer, sizeof(position_buffer), "%d / %d", index + 1, count);
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(position_buffer, bubble.x, bubble.y + 50);

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
git add main/apps/app_more_menu/app_more_menu.h main/apps/app_more_menu/app_more_menu.cpp main/apps/app_more_menu/gui/gui_more_menu.h main/apps/app_more_menu/gui/gui_more_menu.cpp
git add -u main/apps/app_more_menu/more_menu_render_callback.hpp
git commit -m "$(cat <<'EOF'
Turn MoreMenu (MORE) into a read-only stock watchlist viewer

Fetches the watchlist once per app open via STOCK_CLIENT, encoder
rotates through the list (wraps around), no writes to any server.
Adds a proper GUI_Base-derived GUI (GUI_MoreMenu) - the old
placeholder menu never had one, drawing directly via a different
SMOOTH_MENU widget system instead. Class name and launcher wiring
(case 7) unchanged.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Rename the launcher tag and swap the icon

**Files:**
- Modify: `main/apps/launcher/launcher_icons/launcher_icons.h`
- Modify: `main/apps/launcher/launcher_render_callback.hpp`

**Interfaces:** none new — `icon_stock.h` (already committed alongside the design spec) defines `image_data_icon_stock`, a `const uint16_t[1764]` array, same shape as every other icon array.

- [ ] **Step 1: Include the new icon header**

In `main/apps/launcher/launcher_icons/launcher_icons.h`, add (alphabetically, after `icon_sonos.h`):

```cpp
#include "icon_stock.h"
```

(Leave `#include "icon_more.h"` in place — removing unused-but-harmless includes isn't part of this task's scope.)

- [ ] **Step 2: Swap the icon array and rename the tag**

In `main/apps/launcher/launcher_render_callback.hpp`:

Change the `icon_tag_list` entry (currently `"MORE", ""`) to:
```cpp
    "STOCK", "WATCH",
```

Change the `icon_pic_list` entry (currently `image_data_icon_more`) to:
```cpp
    image_data_icon_stock
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
Rename launcher tag MORE to STOCK/WATCH, swap in the stock icon

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

- Launcher shows "STOCK"/"WATCH" tag with the new stock-chart icon in the old MORE slot.
- Opening the app shows "Loading..." briefly, then the first stock.
- Rotating the encoder moves through all the stocks in order; rotating past the last one wraps to the first (and vice versa going backwards).
- Price and % change display correctly; a positive change shows red, a negative change shows green.
- The "X / N" position indicator matches the actual list position.
- If WiFi/the stock server is unreachable, shows an error screen with "TAP TO RETRY"; tapping retries the fetch.
- Quitting (encoder button press-and-release) works from every state.
- Tapping the phone RFID card while this app is open still turns off the lights/fan (RFID_SERVICE regression check).
- Leaving the device idle for 5 minutes while this app is open still turns the backlight off, and touching wakes it without also triggering the retry-tap gesture underneath (IDLE_SCREEN regression check).
