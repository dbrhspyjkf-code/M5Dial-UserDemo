# Fish Tank Light Effect Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add effect-preset control to the fish tank light app — a MODE button toggles whether the encoder adjusts brightness or cycles through the light's 6 built-in effects (read dynamically from HA, not hardcoded).

**Architecture:** Extend `HA_CLIENT::LightState` with `effect`/`effect_list` fields and add `set_light_effect()`, following the exact pattern of the existing brightness functions. `Set_Brightness` gains a `ControlMode` enum and a second touch button; the encoder handler branches on the current mode instead of always adjusting brightness.

**Tech Stack:** ESP-IDF v5.1.3, existing `ha_client`/`wifi_connect_wrap`, C++, `std::vector<std::string>`.

## Global Constraints

- No unit test harness — "test" per task means `idf.py build` succeeds. Full behavior is verified once, at the end, by flashing and manually exercising every control against the real HA light.
- Encoder button press-and-release means "quit the app", unconditionally, in every state.
- Config already exists at `main/apps/app_set_brightness/fishtank_config.h` (gitignored) — no config changes needed for this feature.
- Build environment: `source ~/esp-idf-v5.1.3/export.sh` before any `idf.py` command. Flash port: check `ls /dev/cu.*` each time — confirmed `/dev/cu.usbmodem112401` as of the last flash, but it changes across sessions.
- Effect names are read dynamically from HA's `effect_list` attribute — never hardcode the 6 names anywhere.

---

### Task 1: `ha_client` effect support

**Files:**
- Modify: `main/apps/utilities/ha_client/ha_client.h`
- Modify: `main/apps/utilities/ha_client/ha_client.cpp`

**Interfaces:**
- Consumes: existing `_make_client()`, `_post_json()` private helpers already in `ha_client.cpp`.
- Produces: extended `HA_CLIENT::LightState` (adds `effect`, `effect_list`) and `HA_CLIENT::set_light_effect()`, consumed by Task 2.

- [ ] **Step 1: Extend `LightState` and declare `set_light_effect` in `ha_client.h`**

Replace the existing `LightState` struct definition:

```cpp
    struct LightState
    {
        bool ok = false;
        bool is_on = false;
        int brightness_pct = 0; // 0-100
    };
```
with:
```cpp
    struct LightState
    {
        bool ok = false;
        bool is_on = false;
        int brightness_pct = 0;                // 0-100
        std::string effect;                    // currently active effect name (may be empty)
        std::vector<std::string> effect_list;  // available effect names (may be empty)
    };
```

Add `#include <vector>` to the top of the file, alongside the existing `#include <string>`.

Add this declaration after `set_light_power`'s declaration (before the closing `}` of the namespace):
```cpp
    /**
     * @brief POST /api/services/light/turn_on with
     * {"entity_id": entity_id, "effect": effect_name}
     */
    bool set_light_effect(const char* base_url, const char* token,
                           const char* entity_id, const char* effect_name);
```

- [ ] **Step 2: Update `get_light_state` in `ha_client.cpp` to also parse `effect`/`effect_list`**

Find the existing `get_light_state` function. Inside the `if (result.is_on) { ... }` block, after the existing `brightness` parsing (`cJSON* brightness = ...`), add:

```cpp
                cJSON* effect = cJSON_GetObjectItem(attributes, "effect");
                if (cJSON_IsString(effect))
                {
                    result.effect = effect->valuestring;
                }

                cJSON* effect_list = cJSON_GetObjectItem(attributes, "effect_list");
                if (cJSON_IsArray(effect_list))
                {
                    int count = cJSON_GetArraySize(effect_list);
                    for (int i = 0; i < count; i++)
                    {
                        cJSON* item = cJSON_GetArrayItem(effect_list, i);
                        if (cJSON_IsString(item))
                        {
                            result.effect_list.push_back(item->valuestring);
                        }
                    }
                }
```

So the full `if (result.is_on) { ... }` block in `get_light_state` reads:

```cpp
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

                cJSON* effect = cJSON_GetObjectItem(attributes, "effect");
                if (cJSON_IsString(effect))
                {
                    result.effect = effect->valuestring;
                }

                cJSON* effect_list = cJSON_GetObjectItem(attributes, "effect_list");
                if (cJSON_IsArray(effect_list))
                {
                    int count = cJSON_GetArraySize(effect_list);
                    for (int i = 0; i < count; i++)
                    {
                        cJSON* item = cJSON_GetArrayItem(effect_list, i);
                        if (cJSON_IsString(item))
                        {
                            result.effect_list.push_back(item->valuestring);
                        }
                    }
                }
            }
        }
```

- [ ] **Step 3: Add `set_light_effect` implementation to `ha_client.cpp`**

Add after the existing `set_light_power` function, before the closing `}` of the namespace:

```cpp
    bool set_light_effect(const char* base_url, const char* token,
                           const char* entity_id, const char* effect_name)
    {
        char body[192];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"effect\": \"%s\"}",
                 entity_id, effect_name);

        return _post_json(base_url, token, "/api/services/light/turn_on", body);
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
git add main/apps/utilities/ha_client/ha_client.h main/apps/utilities/ha_client/ha_client.cpp
git commit -m "$(cat <<'EOF'
Add effect support to ha_client's light functions

LightState now also carries the current effect and the light's full
effect_list (read dynamically from HA, never hardcoded), plus a new
set_light_effect() following the same pattern as
set_light_brightness(). Not yet used by any app.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Wire mode-switching and effect cycling into `Set_Brightness`

**Files:**
- Modify: `main/apps/app_set_brightness/app_set_brightness.h`
- Modify: `main/apps/app_set_brightness/app_set_brightness.cpp`
- Modify: `main/apps/app_set_brightness/gui/gui_set_brightness.h`
- Modify: `main/apps/app_set_brightness/gui/gui_set_brightness.cpp`

**Interfaces:**
- Consumes: `HA_CLIENT::LightState` (extended, Task 1), `HA_CLIENT::set_light_effect()` (Task 1).
- Produces: nothing external — `Set_Brightness`'s class name and `getGui()` signature stay unchanged, no launcher changes needed.

- [ ] **Step 1: Update `main/apps/app_set_brightness/app_set_brightness.h`**

Add `#include <vector>` alongside the existing includes. Replace the `SET_BRIGHTNESS::Data_t` struct — add these fields (keep everything already there):

```cpp
            enum class ControlMode { BRIGHTNESS, EFFECT };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::CONNECTING;
                std::string error_message;

                int brightness_pct = 50;
                bool light_on = true;

                bool brightness_dirty = false;
                uint32_t last_brightness_change_ms = 0;

                ControlMode control_mode = ControlMode::BRIGHTNESS;
                std::vector<std::string> effect_list;
                int effect_index = 0;
                bool effect_dirty = false;
                uint32_t last_effect_change_ms = 0;
            };
```

Add one new private method declaration to the `Set_Brightness` class, alongside the existing ones:
```cpp
                void _handle_effect_debounce();
```

- [ ] **Step 2: Update `main/apps/app_set_brightness/app_set_brightness.cpp`**

In `_refresh_state()`, after the existing `_data.light_on = light.is_on;` and brightness-sync block, add effect syncing:

```cpp
    _data.effect_list = light.effect_list;
    if (!_data.effect_list.empty())
    {
        for (size_t i = 0; i < _data.effect_list.size(); i++)
        {
            if (_data.effect_list[i] == light.effect)
            {
                _data.effect_index = (int)i;
                break;
            }
        }
    }
```
So the full `_refresh_state()` becomes:
```cpp
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

    _data.effect_list = light.effect_list;
    if (!_data.effect_list.empty())
    {
        for (size_t i = 0; i < _data.effect_list.size(); i++)
        {
            if (_data.effect_list[i] == light.effect)
            {
                _data.effect_index = (int)i;
                break;
            }
        }
    }
}
```

Replace `_handle_encoder()` entirely:
```cpp
void Set_Brightness::_handle_encoder()
{
    if (_data.state != State::CONTROLLING)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    if (_data.control_mode == ControlMode::BRIGHTNESS)
    {
        _data.brightness_pct += direction * 5;
        if (_data.brightness_pct < 0) _data.brightness_pct = 0;
        if (_data.brightness_pct > 100) _data.brightness_pct = 100;

        _data.brightness_dirty = true;
        _data.last_brightness_change_ms = millis();
    }
    else if (!_data.effect_list.empty())
    {
        int count = (int)_data.effect_list.size();
        _data.effect_index = ((_data.effect_index + direction) % count + count) % count;

        _data.effect_dirty = true;
        _data.last_effect_change_ms = millis();
    }

    _render();
}
```

Add a new method `_handle_effect_debounce()`, right after `_handle_brightness_debounce()`:
```cpp
void Set_Brightness::_handle_effect_debounce()
{
    if (!_data.effect_dirty)
        return;

    if (millis() - _data.last_effect_change_ms < 400)
        return;

    HA_CLIENT::set_light_effect(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN, FISHTANK_ENTITY_ID,
                                 _data.effect_list[_data.effect_index].c_str());
    _data.effect_dirty = false;
}
```

Replace `_handle_touch()` entirely — the on/off hit zone moves from centered to the right half, and a MODE button is added on the left half:
```cpp
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
    else if (_data.state == State::CONTROLLING && y >= 185 && y <= 219)
    {
        if (x >= 40 && x <= 110)
        {
            /* MODE button: only switch to EFFECT if there's something to select */
            if (_data.control_mode == ControlMode::BRIGHTNESS && !_data.effect_list.empty())
            {
                _data.control_mode = ControlMode::EFFECT;
            }
            else
            {
                _data.control_mode = ControlMode::BRIGHTNESS;
            }
            _render();
        }
        else if (x >= 130 && x <= 200)
        {
            _data.light_on = !_data.light_on;
            HA_CLIENT::set_light_power(FISHTANK_HA_BASE_URL, FISHTANK_HA_TOKEN,
                                        FISHTANK_ENTITY_ID, _data.light_on);
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
```

Update `_render()` to pass the new mode/effect info to the GUI — replace it entirely:
```cpp
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
        std::string effect_name = _data.effect_list.empty()
            ? "(no effects)"
            : _data.effect_list[_data.effect_index];

        _gui.renderPage(_data.control_mode == ControlMode::BRIGHTNESS,
                         _data.brightness_pct, effect_name, _data.light_on);
    }
}
```

Update `onRunning()` to call the new debounce handler — replace it entirely:
```cpp
void Set_Brightness::onRunning()
{
    _handle_encoder();
    _handle_brightness_debounce();
    _handle_effect_debounce();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}
```

- [ ] **Step 3: Update `main/apps/app_set_brightness/gui/gui_set_brightness.h`**

Replace the `renderPage` declaration:
```cpp
        void renderPage(int brightnessPct, bool lightOn);
```
with:
```cpp
        /**
         * @brief Render the normal control screen
         *
         * @param brightnessMode true = show/adjust brightness, false = show/adjust effect
         * @param brightnessPct 0-100 (shown when brightnessMode is true)
         * @param effectName current effect name, or "(no effects)" (shown when brightnessMode is false)
         * @param lightOn current on/off state (affects the ON/OFF button label)
         */
        void renderPage(bool brightnessMode, int brightnessPct,
                         const std::string& effectName, bool lightOn);
```

- [ ] **Step 4: Update `main/apps/app_set_brightness/gui/gui_set_brightness.cpp`**

Replace the `renderPage` function entirely:

```cpp
void GUI_SetBrightness::renderPage(bool brightnessMode, int brightnessPct,
                                    const std::string& effectName, bool lightOn)
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

    /* Small mode indicator, above the big value */
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString(brightnessMode ? "BRIGHTNESS" : "EFFECT", bubble.x, bubble.y - 68);

    /* Big value: brightness percent, or the current effect name */
    _canvas->setTextSize(brightnessMode ? 3 : 2);
    if (brightnessMode)
    {
        char string_buffer[24];
        snprintf(string_buffer, sizeof(string_buffer), "%d%%", brightnessPct);
        _canvas->drawCenterString(string_buffer, bubble.x, bubble.y - 56);
    }
    else
    {
        std::string display_effect = effectName;
        while (_canvas->textWidth(display_effect.c_str()) > 200 && display_effect.size() > 3)
        {
            display_effect = display_effect.substr(0, display_effect.size() - 4) + "...";
        }
        _canvas->drawCenterString(display_effect.c_str(), bubble.x, bubble.y - 50);
    }

    _canvas->setTextSize(1);
    _canvas->drawCenterString("FISH LIGHT", bubble.x, bubble.y + 26);

    /* MODE and ON/OFF buttons, side by side below the bubble (bubble bottom
       edge is at bubble.y + bubble.height/2 = 190) */
    int btn_y = 202;
    int btn_height = 24;

    _canvas->setTextSize(1);

    const char* mode_label = "MODE";
    int mode_text_w = _canvas->textWidth(mode_label);
    int mode_text_h = _canvas->fontHeight();
    int mode_btn_width = mode_text_w + 30;
    if (mode_btn_width < 60) mode_btn_width = 60;
    int mode_btn_x = 75;

    _canvas->fillSmoothRoundRect(mode_btn_x - mode_btn_width / 2, btn_y - btn_height / 2,
                                  mode_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(mode_label, mode_btn_x, btn_y - mode_text_h / 2);

    const char* power_label = lightOn ? "ON" : "OFF";
    int power_text_w = _canvas->textWidth(power_label);
    int power_text_h = _canvas->fontHeight();
    int power_btn_width = power_text_w + 30;
    if (power_btn_width < 60) power_btn_width = 60;
    int power_btn_x = 165;

    _canvas->fillSmoothRoundRect(power_btn_x - power_btn_width / 2, btn_y - btn_height / 2,
                                  power_btn_width, btn_height, btn_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString(power_label, power_btn_x, btn_y - power_text_h / 2);

    _draw_quit_button();
    _canvas->pushSprite(0, 0);
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
Add effect-preset control to the fish tank light app

New MODE touch button toggles whether the encoder adjusts brightness
or cycles through the light's effect presets (read dynamically from
HA's effect_list, never hardcoded). Bubble content and mode label
switch accordingly; ON/OFF button moved from centered to share the
row with the new MODE button.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Flash and manually verify

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

On the device, open the fish tank light app (launcher slot 3, "BRIGHTNESS"/"SET"):
- [ ] Opens in BRIGHTNESS mode as before, bubble shows a small "BRIGHTNESS" label above the percentage
- [ ] Tap MODE → bubble switches to show "EFFECT" label and the current effect name; encoder now cycles effects instead of brightness
- [ ] Rotating the encoder in EFFECT mode actually changes the fish tank light's effect within ~400ms of stopping, and wraps around cleanly at both ends of the list (6 effects → rotating past the last one goes back to the first, and vice versa)
- [ ] Tap MODE again → back to BRIGHTNESS mode, encoder adjusts brightness again exactly as before this feature
- [ ] ON/OFF button (now on the right side, not centered) still works from either mode
- [ ] Quit (encoder button) works from every state

- [ ] **Step 3: Fix any issues found, rebuild, reflash, recheck**

Same iterative loop as every other app built this session — adjust touch-zone/layout constants in `gui_set_brightness.cpp`/`app_set_brightness.cpp` based on what's actually observed. Commit fixes:

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_set_brightness
git commit -m "$(cat <<'EOF'
Adjust fish tank light effect UI after on-device testing

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Skip this commit if nothing needed adjusting.)

---

## Self-Review

**Spec coverage:**
- Effect names read dynamically from `effect_list`, never hardcoded — Task 1 (`get_light_state` parses `attributes.effect_list`), Task 2 (`_data.effect_list` populated from `light.effect_list`, never a literal effect name in code). ✓
- MODE button toggles encoder target — Task 2 (`_handle_touch`'s MODE branch, `_handle_encoder`'s `control_mode` branch). ✓
- Effect cycling wraps correctly — Task 2 (`((_data.effect_index + direction) % count + count) % count`, standard wrap-safe modulo for negative results). ✓
- Bubble content/mode indicator switches with mode — Task 2 (`renderPage`'s `brightnessMode` parameter). ✓
- ON/OFF button still present, repositioned — Task 2 (`_handle_touch`'s x-range for the button moved from `85-155` centered to `130-200` on the right; `gui_set_brightness.cpp`'s `power_btn_x = 165`). ✓
- Graceful handling of a light with no effects (`effect_list` empty) — Task 2 (`_handle_touch`'s MODE branch checks `!_data.effect_list.empty()` before switching to EFFECT mode; `_render()`'s `"(no effects)"` fallback). ✓
- Testing/verification plan — Task 3. ✓

**Placeholder scan:** no TBD/TODO; all code blocks are complete, runnable code shown in full (including the full modified `_refresh_state`, `_handle_encoder`, `_handle_touch`, `_render`, `onRunning` functions, not just diffs, since a task's implementer needs the complete function body).

**Type consistency:** `HA_CLIENT::LightState::effect_list` is `std::vector<std::string>` in both Task 1's struct definition and Task 2's `_data.effect_list = light.effect_list;` assignment (matching `SET_BRIGHTNESS::Data_t::effect_list`, also `std::vector<std::string>`). `GUI_SetBrightness::renderPage`'s new signature (`bool, int, const std::string&, bool`) matches between Task 2's header declaration, its `.cpp` definition, and its call site in `_render()`.
