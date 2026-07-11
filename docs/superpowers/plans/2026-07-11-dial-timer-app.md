# M5Dial Timer App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a touch-controlled MM:SS countdown timer app to M5Dial-UserDemo, launched as the 9th icon in the existing circular launcher.

**Architecture:** New `AppTimer` (Mooncake `APP_BASE`) + `GUI_Timer` (`GUI_Base`) pair, following the exact file/naming pattern of the existing `app_set_brightness`. State machine (EDIT → RUNNING → PAUSED → DONE) lives in `AppTimer`; `GUI_Timer` is a pure renderer taking primitive values. Registered into the launcher's 8→9 icon slot.

**Tech Stack:** ESP-IDF v5.1.3 (installed at `~/esp-idf-v5.1.3`), C++ (gnu++2b), LovyanGFX for drawing, existing HAL (`encoder`, `tp`, `buzz`, `canvas`).

## Global Constraints

- No unit test harness exists in this codebase — "test" for each task means `idf.py build` succeeds (catches compile errors). Full behavior is verified once, at the end, by flashing and manually exercising every state.
- Encoder button press-and-release means "quit the app", unconditionally, in every state — this is a project-wide convention (see every existing app's `onRunning`), not something this feature may override.
- No persistence: state resets to defaults every time the app is opened.
- Build environment: `source ~/esp-idf-v5.1.3/export.sh` before any `idf.py` command. Target device port for flashing: `/dev/cu.usbmodem1101` (confirm with `ls /dev/cu.*` — it can change on replug).

---

### Task 1: AppTimer + GUI_Timer (core state machine and rendering)

**Files:**
- Create: `main/apps/app_timer/app_timer.h`
- Create: `main/apps/app_timer/app_timer.cpp`
- Create: `main/apps/app_timer/gui/gui_timer.h`
- Create: `main/apps/app_timer/gui/gui_timer.cpp`

**Interfaces:**
- Consumes: `HAL::HAL` (`encoder.wasMoved(bool)`, `encoder.getDirection()`, `encoder.btn.read()`, `tp.isTouched()`, `tp.update()`, `tp.getTouchPointBuffer()` → `{touch_num,x,y}`, `buzz.tone(freq,duration)`, `canvas` (`LGFX_Sprite*`)); `GUI_Base` (`_canvas`, `_theme_color`, `setCanvas`/`init`, `_draw_quit_button()`, `_draw_top_icon()`); `MOONCAKE::APP_BASE` (`setAppName`, `setAllowBgRunning`, `getUserData()`, `destroyApp()`).
- Produces: `MOONCAKE::USER_APP::AppTimer` class and `MOONCAKE::USER_APP::TIMER::Data_t` struct, consumed by Task 3 (`launcher.h`/`launcher.cpp`).

This task is self-contained: `main/CMakeLists.txt` globs all `.cpp`/`.c` under `apps/`, so these new files get compiled into the build the moment they exist, even before the launcher references `AppTimer`. That makes "does it compile" a real per-task check here.

- [ ] **Step 1: Write `main/apps/app_timer/gui/gui_timer.h`**

```cpp
/**
 * @file gui_timer.h
 * @brief Renderer for the Timer app. Pure display — takes primitive
 * values, draws them. No state lives here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_Timer : public GUI_Base
{
    private:

    public:
        void init() override;

        /**
         * @brief Render the timer page
         *
         * @param displayMinutes minutes value to show
         * @param displaySeconds seconds value to show
         * @param selectedField  0 = minutes highlighted, 1 = seconds highlighted, -1 = no highlight
         * @param pillLabel      text for the bottom pill button (START/PAUSE/RESUME/RESET)
         * @param showDigits     false to blank the digits (used for the DONE-state blink)
         */
        void renderPage(int displayMinutes, int displaySeconds, int selectedField,
                         const std::string& pillLabel, bool showDigits);
};
```

- [ ] **Step 2: Write `main/apps/app_timer/gui/gui_timer.cpp`**

```cpp
/**
 * @file gui_timer.cpp
 */
#include "gui_timer.h"
#include <cstdio>


void GUI_Timer::init()
{
}


void GUI_Timer::renderPage(int displayMinutes, int displaySeconds, int selectedField,
                            const std::string& pillLabel, bool showDigits)
{
    _canvas->fillScreen(_theme_color);

    /* Icon */
    _draw_top_icon();

    /* Bubble */
    BasicObeject_t bubble;
    bubble.x = 120;
    bubble.y = 105;
    bubble.width = 240;
    bubble.height = 110;
    _canvas->fillSmoothRoundRect(bubble.x - bubble.width / 2, bubble.y - bubble.height / 2,
                                  bubble.width, bubble.height, 30, TFT_WHITE);

    /* "TIMER" label */
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(1);
    _canvas->drawCenterString("TIMER", bubble.x, bubble.y - 34);

    /* Digits (MM:SS), blanked when showDigits is false (DONE-state blink) */
    if (showDigits)
    {
        char mm_buf[3];
        char ss_buf[3];
        snprintf(mm_buf, sizeof(mm_buf), "%02d", displayMinutes);
        snprintf(ss_buf, sizeof(ss_buf), "%02d", displaySeconds);

        _canvas->setTextColor(TFT_BLACK);
        _canvas->setTextSize(3);
        _canvas->drawRightString(mm_buf, bubble.x - 10, bubble.y - 10);
        _canvas->drawCenterString(":", bubble.x, bubble.y - 10);
        _canvas->drawString(ss_buf, bubble.x + 10, bubble.y - 10);
    }

    /* Selected-field underline */
    if (selectedField == 0)
    {
        _canvas->fillSmoothRoundRect(70, bubble.y + 8, 40, 4, 2, _theme_color);
    }
    else if (selectedField == 1)
    {
        _canvas->fillSmoothRoundRect(130, bubble.y + 8, 40, 4, 2, _theme_color);
    }

    /* Pill button (START / PAUSE / RESUME / RESET) */
    int pill_x = 120;
    int pill_y = 190;
    int pill_width = 100;
    int pill_height = 30;
    _canvas->fillSmoothRoundRect(pill_x - pill_width / 2, pill_y - pill_height / 2,
                                  pill_width, pill_height, pill_height / 2, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->setTextSize(2);
    _canvas->drawCenterString(pillLabel.c_str(), pill_x, pill_y - 8);

    /* Quit indicator */
    _draw_quit_button();

    _canvas->pushSprite(0, 0);
}
```

- [ ] **Step 3: Write `main/apps/app_timer/app_timer.h`**

```cpp
/**
 * @file app_timer.h
 * @brief MM:SS countdown timer. Touch selects/starts/pauses/resets;
 * encoder rotate adjusts the selected field; encoder button quits
 * (project-wide convention, same as every other app here).
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_timer.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace TIMER
        {
            enum class Field { MINUTES, SECONDS };
            enum class State { EDIT, RUNNING, PAUSED, DONE };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;

                State state = State::EDIT;
                Field selected_field = Field::SECONDS;

                /* Last-edited duration, restored when RESET from DONE */
                int set_minutes = 5;
                int set_seconds = 0;

                /* Live countdown value while RUNNING/PAUSED/DONE */
                int remaining_seconds = 300;

                uint32_t last_tick_ms = 0;
                uint32_t last_blink_ms = 0;
                bool blink_on = true;
            };
        }

        class AppTimer : public APP_BASE
        {
            private:
                const char* _tag = "AppTimer";

                void _handle_encoder();
                void _handle_touch();
                void _handle_tick();
                void _render();
                std::string _pill_label();

            public:
                TIMER::Data_t _data;
                GUI_Timer _gui;

                GUI_Base* getGui() override { return &_gui; }

                void onSetup();
                void onCreate();
                void onRunning();
                void onDestroy();
        };
    }
}
```

- [ ] **Step 4: Write `main/apps/app_timer/app_timer.cpp`**

```cpp
/**
 * @file app_timer.cpp
 */
#include "app_timer.h"
#include "../common_define.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::TIMER;


void AppTimer::onSetup()
{
    setAppName("AppTimer");
    setAllowBgRunning(false);

    TIMER::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void AppTimer::onCreate()
{
    _log("onCreate");
    _render();
}


std::string AppTimer::_pill_label()
{
    switch (_data.state)
    {
        case State::EDIT:    return "START";
        case State::RUNNING: return "PAUSE";
        case State::PAUSED:  return "RESUME";
        case State::DONE:    return "RESET";
    }
    return "START";
}


void AppTimer::_handle_encoder()
{
    if (_data.state != State::EDIT && _data.state != State::PAUSED)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int step = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    if (_data.selected_field == Field::MINUTES)
    {
        _data.set_minutes += step;
        if (_data.set_minutes < 0)  _data.set_minutes = 99;
        if (_data.set_minutes > 99) _data.set_minutes = 0;
    }
    else
    {
        _data.set_seconds += step;
        if (_data.set_seconds < 0)  _data.set_seconds = 59;
        if (_data.set_seconds > 59) _data.set_seconds = 0;
    }

    _render();
}


void AppTimer::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    _data.hal->tp.update();
    int x = _data.hal->tp.getTouchPointBuffer().x;
    int y = _data.hal->tp.getTouchPointBuffer().y;

    /* Digit row tap: select field (only while editable) */
    if ((_data.state == State::EDIT || _data.state == State::PAUSED) &&
        y >= 75 && y <= 115)
    {
        _data.selected_field = (x < 120) ? Field::MINUTES : Field::SECONDS;
        _render();
    }

    /* Pill button tap */
    if (x >= 70 && x <= 170 && y >= 175 && y <= 205)
    {
        switch (_data.state)
        {
            case State::EDIT:
                _data.remaining_seconds = _data.set_minutes * 60 + _data.set_seconds;
                if (_data.remaining_seconds > 0)
                {
                    _data.state = State::RUNNING;
                    _data.last_tick_ms = millis();
                }
                break;

            case State::RUNNING:
                _data.state = State::PAUSED;
                _data.set_minutes = _data.remaining_seconds / 60;
                _data.set_seconds = _data.remaining_seconds % 60;
                break;

            case State::PAUSED:
                _data.remaining_seconds = _data.set_minutes * 60 + _data.set_seconds;
                _data.state = State::RUNNING;
                _data.last_tick_ms = millis();
                break;

            case State::DONE:
                _data.state = State::EDIT;
                _data.blink_on = true;
                break;
        }
        _render();
    }

    /* Debounce: wait for release so one tap isn't read as many */
    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void AppTimer::_handle_tick()
{
    if (_data.state == State::RUNNING)
    {
        if (millis() - _data.last_tick_ms >= 1000)
        {
            _data.last_tick_ms += 1000;
            _data.remaining_seconds -= 1;

            if (_data.remaining_seconds <= 0)
            {
                _data.remaining_seconds = 0;
                _data.state = State::DONE;
                _data.blink_on = true;
                _data.last_blink_ms = millis();
                _data.hal->buzz.tone(4000, 200);
            }

            _render();
        }
    }
    else if (_data.state == State::DONE)
    {
        if (millis() - _data.last_blink_ms >= 500)
        {
            _data.last_blink_ms = millis();
            _data.blink_on = !_data.blink_on;
            _render();
        }
    }
}


void AppTimer::_render()
{
    int display_minutes;
    int display_seconds;

    if (_data.state == State::RUNNING || _data.state == State::DONE)
    {
        display_minutes = _data.remaining_seconds / 60;
        display_seconds = _data.remaining_seconds % 60;
    }
    else
    {
        display_minutes = _data.set_minutes;
        display_seconds = _data.set_seconds;
    }

    int selected_field = -1;
    if (_data.state == State::EDIT || _data.state == State::PAUSED)
    {
        selected_field = (_data.selected_field == Field::MINUTES) ? 0 : 1;
    }

    bool show_digits = (_data.state != State::DONE) || _data.blink_on;

    _gui.renderPage(display_minutes, display_seconds, selected_field, _pill_label(), show_digits);
}


void AppTimer::onRunning()
{
    _handle_encoder();
    _handle_touch();
    _handle_tick();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void AppTimer::onDestroy()
{
    _log("onDestroy");
}
```

- [ ] **Step 5: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.` — no errors. (New files are picked up automatically by the `apps/*.cpp` glob in `main/CMakeLists.txt`; nothing references `AppTimer` yet so it just needs to compile standalone.)

- [ ] **Step 6: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_timer
git commit -m "$(cat <<'EOF'
Add AppTimer: MM:SS countdown timer state machine + renderer

Touch-driven field select/start/pause/reset, encoder adjusts the
selected field, encoder button quits (matches every other app's
convention). Not yet wired into the launcher.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Timer icon asset

**Files:**
- Create: `main/apps/launcher/launcher_icons/icon_timer.h`
- Modify: `main/apps/launcher/launcher_icons/launcher_icons.h`

**Interfaces:**
- Produces: `static const uint16_t image_data_icon_timer[1764]`, consumed by Task 3's `icon_pic_list` in `launcher_render_callback.hpp`.

Existing icons are 42×42 RGB565 arrays, generated from a PNG colored to match that icon's assigned background circle (white glyph on the accent color, transparent background flattened to black — `TFT_BLACK` is the transparency key used by `pushRotateZoom`). This task generates a stopwatch glyph in the same style, in a new accent color (`0xB565F3`, distinct from the 8 existing `icon_color_list` colors) that Task 3 will assign to the Timer slot.

- [ ] **Step 1: Generate the icon with Python (PIL)**

Run this from `main/apps/launcher/launcher_icons/`:

```bash
cd ~/Projects/M5Dial-UserDemo/main/apps/launcher/launcher_icons
python3 - <<'EOF'
from PIL import Image, ImageDraw

SIZE = 42
SUPER = 4
BIG = SIZE * SUPER

ACCENT = (0xB5, 0x65, 0xF3)
WHITE = (255, 255, 255)

img = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)

cx = cy = BIG / 2
r = BIG * 0.42

draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=ACCENT + (255,))

ring_r = r * 0.72
draw.ellipse([cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r],
             outline=WHITE + (255,), width=int(BIG * 0.045))

knob_w = BIG * 0.14
draw.rectangle([cx - knob_w / 2, cy - r - BIG * 0.02, cx + knob_w / 2, cy - r + BIG * 0.10],
               fill=WHITE + (255,))

draw.line([cx, cy, cx, cy - ring_r * 0.7], fill=WHITE + (255,), width=int(BIG * 0.045))
draw.line([cx, cy, cx + ring_r * 0.5, cy], fill=WHITE + (255,), width=int(BIG * 0.045))
draw.ellipse([cx - BIG * 0.02, cy - BIG * 0.02, cx + BIG * 0.02, cy + BIG * 0.02], fill=WHITE + (255,))

img = img.resize((SIZE, SIZE), Image.LANCZOS)

bg = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 255))
bg.alpha_composite(img)
bg = bg.convert("RGB")

def to565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

values = [to565(r, g, b) for (r, g, b) in bg.getdata()]

with open("icon_timer.h", "w") as f:
    f.write("#pragma once\n#include <stdint.h>\n\n\n")
    f.write(f"static const uint16_t image_data_icon_timer[{len(values)}] = {{\n")
    for i in range(0, len(values), 16):
        row = values[i:i + 16]
        f.write("    " + ", ".join(f"0x{v:04x}" for v in row) + ",\n")
    f.write("};\n")

bg.save("icon_timer_preview.png")
print("wrote", len(values), "values")
EOF
```
Expected output: `wrote 1764 values`. This creates `icon_timer.h` and a `icon_timer_preview.png` you can open to sanity-check the glyph looks like a stopwatch (circle + ring + top knob + two hands).

- [ ] **Step 2: Register the new icon header**

Edit `main/apps/launcher/launcher_icons/launcher_icons.h`, add the include (alphabetical, matching the existing order):

```cpp
#include "icon_rtc.h"
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
Expected: `Project build complete.` (The array is unused until Task 3 references it — `-Wno-error=unused-variable` in this project's build flags means that's a warning, not a build failure.)

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/launcher/launcher_icons/icon_timer.h main/apps/launcher/launcher_icons/icon_timer_preview.png main/apps/launcher/launcher_icons/launcher_icons.h
git commit -m "$(cat <<'EOF'
Add stopwatch icon asset for the Timer app

42x42 RGB565, generated from a simple stopwatch glyph in a new
accent color (0xB565F3) not used by any existing launcher icon.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Wire AppTimer into the launcher (9th icon)

**Files:**
- Modify: `main/apps/launcher/launcher_render_callback.hpp`
- Modify: `main/apps/launcher/launcher.h`
- Modify: `main/apps/launcher/launcher.cpp`

**Interfaces:**
- Consumes: `MOONCAKE::USER_APP::AppTimer` (Task 1), `image_data_icon_timer` (Task 2).
- Produces: end-to-end working 9th launcher icon that opens the Timer app.

- [ ] **Step 1: Bump `ICON_NUM` and extend the icon arrays**

In `main/apps/launcher/launcher_render_callback.hpp`, change:

```cpp
#define ICON_NUM                    8
```
to:
```cpp
#define ICON_NUM                    9
```

Then extend `icon_color_list` (add one entry, matching the icon's accent color from Task 2):

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
    0xB565F3
};
```

Extend `icon_tag_list` (add one pair):

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
    "TIMER", "SET"
};
```

Extend `icon_pic_list` (add one entry):

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
    image_data_icon_timer
};
```

- [ ] **Step 2: Include the new app header in `launcher.h`**

Add, alongside the other app includes:

```cpp
#include "../app_more_menu/app_more_menu.h"
#include "../app_ble_server/app_ble_server.h"
#include "../app_timer/app_timer.h"
```

- [ ] **Step 3: Add the switch case in `launcher.cpp`**

In `Launcher::_app_open_callback`, add a case 8 branch:

```cpp
        case 7:
            app_ptr = new MOONCAKE::USER_APP::MoreMenu;
            break;
        case 8:
            app_ptr = new MOONCAKE::USER_APP::AppTimer;
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
Wire Timer app into the launcher as the 9th icon

Bumps ICON_NUM 8->9 (the circular menu grid already had 10 slots,
this fills one of the two previously-empty ones) and dispatches to
AppTimer from the launcher's app-open switch.

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
idf.py -p /dev/cu.usbmodem1101 flash
```
Expected: flashing completes, `Hash of data verified.` for both writes, device hard-resets.

- [ ] **Step 2: Manual verification checklist**

On the device:
- [ ] Rotate the launcher to the 9th icon (purple, tagged "TIMER"/"SET") and open it — bubble with "05:00" and a "TIMER" label, "START" pill button, "<" at the bottom.
- [ ] Tap the left half of the digits ("05") → underline moves under the minutes; rotate encoder → minutes value changes (wraps 0↔99).
- [ ] Tap the right half ("00") → underline moves under seconds; rotate encoder → seconds value changes (wraps 0↔59).
- [ ] Set a short duration (e.g. 00:05) and tap START → pill becomes "PAUSE", digits count down every second, tapping the digits while running does nothing.
- [ ] Tap PAUSE mid-countdown → pill becomes "RESUME", digits stop; tap a digit half → underline reappears and encoder now adjusts the *remaining* time; tap RESUME → countdown continues.
- [ ] Let it reach 00:00 → buzzer beeps once, digits blink, pill becomes "RESET".
- [ ] Tap RESET → returns to the originally-set duration (not 00:00), pill back to "START".
- [ ] From each of EDIT / RUNNING / PAUSED / DONE, press and release the encoder button → app quits back to the launcher every time.

- [ ] **Step 3: Fix any visual/layout issues found**

The pixel positions in `gui_timer.cpp` (bubble size, digit position, pill button rect, touch hit-test rectangles in `app_timer.cpp::_handle_touch`) are starting values, not guaranteed pixel-perfect on first flash. If touch zones feel off or text overlaps, adjust the constants directly in those two files, rebuild, reflash, and recheck the specific item that was wrong. Commit any fixes:

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/app_timer main/apps/launcher/launcher_icons/icon_timer_preview.png
git commit -m "$(cat <<'EOF'
Adjust Timer app layout/touch-zone constants after on-device testing

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Skip this commit if nothing needed adjusting.)

---

## Self-Review

**Spec coverage:**
- Scope (MM:SS only, no persistence, no HA) — Task 1's `Data_t` has no hour field, no NVS/storage writes. ✓
- Deviation from reference (touch-driven, not double-click; encoder button always quits) — Task 1's `_handle_touch`/`onRunning`. ✓
- File structure / naming — Task 1 matches exactly. ✓
- Launcher integration (ICON_NUM, arrays, include, switch case) — Task 3. ✓
- New icon asset — Task 2. ✓
- Full state machine (EDIT/RUNNING/PAUSED/DONE + transitions) — Task 1's `_handle_touch`/`_handle_tick`. ✓
- Buzzer on completion — `_data.hal->buzz.tone(4000, 200)` in `_handle_tick`. ✓
- Testing/verification plan — Task 4. ✓

**Placeholder scan:** no TBD/TODO; all code blocks are complete, runnable code, not descriptions.

**Type consistency:** `TIMER::Field`/`TIMER::State` enums defined in Task 1's `app_timer.h`, used consistently in `app_timer.cpp`; `GUI_Timer::renderPage` signature in Task 1's `gui_timer.h` matches its call site in `app_timer.cpp::_render` and its definition in `gui_timer.cpp`. `image_data_icon_timer` name matches between Task 2's generator script and Task 3's `icon_pic_list` entry.
