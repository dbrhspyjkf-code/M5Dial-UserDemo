# Idle Screen Sleep Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** After 5 minutes of no touch/encoder/button activity, turn the screen backlight off; the next touch/rotation/button press wakes it back up without also triggering an action.

**Architecture:** A new `IDLE_SCREEN::tick(HAL::HAL*)` function is called once per iteration in the project's only 2 `while(1)` loop sites — `main.cpp`'s launcher loop and `Launcher::_simple_app_manager` (which drives every app) — covering the Launcher and all 10 apps without touching any individual app file.

**Tech Stack:** ESP-IDF v5.1.3, existing `HAL::HAL` (display/tp/encoder), no new dependencies.

## Global Constraints

- 5-minute timeout is a hardcoded constant (`300000` ms), not configurable.
- Brightness: 128 = on (matches `hal.cpp`'s existing boot-animation steady-state value), 0 = off.
- Must never call `hal->encoder.wasMoved()` except inside the wake-consuming branch — calling it on every tick would silently eat rotation events before the open app's own `onRunning()` sees them, breaking every encoder-driven control in the project (confirmed by reading `ESP32Encoder::wasMoved()`'s implementation, which mutates its own internal `_last_count` as a side effect).

---

### Task 1: Create the `IDLE_SCREEN` utility

**Files:**
- Create: `main/apps/utilities/idle_screen/idle_screen.h`
- Create: `main/apps/utilities/idle_screen/idle_screen.cpp`

**Interfaces:**
- Produces: `bool IDLE_SCREEN::tick(HAL::HAL* hal)` — call once per loop iteration, before calling `onRunning()`. Returns `true` only on the exact cycle where it just consumed a wake-from-sleep gesture (caller should skip `onRunning()` that one cycle); `false` every other cycle (caller calls `onRunning()` normally). Consumed by Task 2 (`main.cpp`) and Task 3 (`launcher.cpp`).

- [ ] **Step 1: Write `idle_screen.h`**

```cpp
/**
 * @file idle_screen.h
 * @brief After 5 minutes with no touch/encoder/button activity, turns
 * the screen backlight off to save power. The next touch, encoder
 * rotation, or encoder-button press wakes it back up. Hooked into the
 * project's 2 shared while(1) loop sites (main.cpp's launcher loop and
 * Launcher::_simple_app_manager, which drives every app) instead of
 * any individual app, so no app file needs to change.
 */
#pragma once
#include "../../../hal/hal.h"

namespace IDLE_SCREEN
{
    /**
     * @brief Call once per loop iteration, before calling onRunning().
     *
     * @return true only on the exact cycle where this call just woke
     * the screen from sleep and consumed the gesture that woke it
     * (touch release / button release / pending encoder rotation) -
     * the caller should skip onRunning() for that one cycle so the
     * wake gesture can't also trigger an action. Returns false on
     * every other cycle (screen already on, or already asleep with no
     * new activity yet) - the caller should call onRunning() as usual.
     */
    bool tick(HAL::HAL* hal);
}
```

- [ ] **Step 2: Write `idle_screen.cpp`**

```cpp
/**
 * @file idle_screen.cpp
 */
#include "idle_screen.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))

namespace IDLE_SCREEN
{
    static const uint32_t IDLE_TIMEOUT_MS = 5 * 60 * 1000;
    static const int ON_BRIGHTNESS = 128;

    static bool s_initialized = false;
    static uint32_t s_last_activity_ms = 0;
    static int64_t s_last_encoder_count = 0;
    static bool s_screen_on = true;

    bool tick(HAL::HAL* hal)
    {
        if (!s_initialized)
        {
            s_last_activity_ms = millis();
            s_last_encoder_count = hal->encoder.getCount();
            s_initialized = true;
        }

        bool touched = hal->tp.isTouched();

        /* Read the raw count directly (no side effects) instead of
           wasMoved(), which mutates its own internal _last_count and
           would otherwise silently consume rotation before the open
           app's own onRunning() gets a chance to see it. */
        int64_t current_count = hal->encoder.getCount();
        bool encoder_moved = (current_count != s_last_encoder_count);
        s_last_encoder_count = current_count;

        bool button_pressed = !hal->encoder.btn.read();

        bool activity = touched || encoder_moved || button_pressed;

        if (activity)
        {
            s_last_activity_ms = millis();

            if (!s_screen_on)
            {
                hal->display.setBrightness(ON_BRIGHTNESS);
                s_screen_on = true;

                /* Absorb the whole gesture that woke the screen, same
                   "wake, don't act" behavior as a phone lock screen. */
                if (touched)
                {
                    while (hal->tp.isTouched())
                    {
                        hal->tp.update();
                        delay(5);
                    }
                }
                if (button_pressed)
                {
                    while (!hal->encoder.btn.read())
                        delay(5);
                }
                if (encoder_moved)
                {
                    /* Absorb the pending rotation into wasMoved()'s own
                       tracking so the open app's next wasMoved() call
                       doesn't see leftover movement from before wake. */
                    hal->encoder.wasMoved(true);
                }

                s_last_activity_ms = millis();
                return true;
            }

            return false;
        }

        if (s_screen_on && (millis() - s_last_activity_ms > IDLE_TIMEOUT_MS))
        {
            hal->display.setBrightness(0);
            s_screen_on = false;
        }

        return false;
    }
}
```

- [ ] **Step 3: Build to verify it compiles**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py build
```
Expected: `Project build complete.` (This file isn't called from anywhere yet, so this just verifies the new files compile standalone.)

- [ ] **Step 4: Commit**

```bash
cd ~/Projects/M5Dial-UserDemo
git add main/apps/utilities/idle_screen
git commit -m "$(cat <<'EOF'
Add IDLE_SCREEN: backlight-off after 5 minutes of no activity

Tracks touch/encoder/button activity and turns the backlight off
after 5 minutes idle; next activity wakes it and absorbs that one
gesture so it can't also trigger an action. Not yet called from
anywhere - no behavior change until it's wired into the 2 shared
while(1) loop sites.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Wire into `main.cpp`'s launcher loop

**Files:**
- Modify: `main/main.cpp`

**Interfaces:**
- Consumes: `IDLE_SCREEN::tick(HAL::HAL*)` (Task 1).

- [ ] **Step 1: Add the include**

In `main/main.cpp`, add this include alongside the existing ones:

```cpp
#include "apps/utilities/idle_screen/idle_screen.h"
```

- [ ] **Step 2: Wrap the launcher's while(1) loop**

Change:
```cpp
    /* Start launcher */
    MOONCAKE::USER_APP::Launcher app_launcher;
    app_launcher.setUserData((void*)&hal);
    app_launcher.onSetup();
    app_launcher.onCreate();
    while (1)
    {
        app_launcher.onRunning();
    }
}
```
to:
```cpp
    /* Start launcher */
    MOONCAKE::USER_APP::Launcher app_launcher;
    app_launcher.setUserData((void*)&hal);
    app_launcher.onSetup();
    app_launcher.onCreate();
    while (1)
    {
        if (!IDLE_SCREEN::tick(&hal))
        {
            app_launcher.onRunning();
        }
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
git add main/main.cpp
git commit -m "$(cat <<'EOF'
Wire IDLE_SCREEN into the launcher's main loop

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Wire into `Launcher::_simple_app_manager` (covers every app)

**Files:**
- Modify: `main/apps/launcher/launcher.cpp`

**Interfaces:**
- Consumes: `IDLE_SCREEN::tick(HAL::HAL*)` (Task 1).

- [ ] **Step 1: Add the include**

In `main/apps/launcher/launcher.cpp`, add alongside the existing includes:

```cpp
#include "../utilities/idle_screen/idle_screen.h"
```

- [ ] **Step 2: Wrap the app-running while(1) loop**

Change:
```cpp
void Launcher::_simple_app_manager(MOONCAKE::APP_BASE* app)
{
    app->setUserData((void*)_data.hal);
    app->onSetup();
    app->onCreate();
    while (1)
    {
        app->onRunning();
        if (app->isGoingDestroy())
        {
            app->resetGoingDestroyFlag();
            app->onDestroy();
            break;
        }

        // if (_data.hal->encoder.btn.pressed())
        // {
        //     /* Hold until button release */
        //     while (!_data.hal->encoder.btn.read());
        //     break;
        // }
    }
}
```
to:
```cpp
void Launcher::_simple_app_manager(MOONCAKE::APP_BASE* app)
{
    app->setUserData((void*)_data.hal);
    app->onSetup();
    app->onCreate();
    while (1)
    {
        if (!IDLE_SCREEN::tick(_data.hal))
        {
            app->onRunning();
        }
        if (app->isGoingDestroy())
        {
            app->resetGoingDestroyFlag();
            app->onDestroy();
            break;
        }

        // if (_data.hal->encoder.btn.pressed())
        // {
        //     /* Hold until button release */
        //     while (!_data.hal->encoder.btn.read());
        //     break;
        // }
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
git add main/apps/launcher/launcher.cpp
git commit -m "$(cat <<'EOF'
Wire IDLE_SCREEN into _simple_app_manager, covering every app

Every app funnels through this one loop, so this single change
enables idle-sleep for all 10 apps without touching any of them.

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

- [ ] **Step 2: Flash (confirm with the user before running — standing project convention)**

```bash
source ~/esp-idf-v5.1.3/export.sh
cd ~/Projects/M5Dial-UserDemo
idf.py -p <PORT> flash
```

- [ ] **Step 3: Manual verification checklist**

Since waiting 5 real minutes per test is slow, first verify the mechanism quickly by temporarily confirming behavior at the launcher, then rely on the 5-minute wait for final confirmation (do not permanently shorten the timeout for testing - test at the real value):

- Leave the device untouched (launcher screen) for 5 minutes -> backlight turns off.
- Touch the screen -> backlight comes back on; that touch does NOT open the app under the cursor (it's absorbed).
- A second, separate touch on an app icon after wake -> opens that app normally.
- Repeat the untouched-5-minutes test with an app open (e.g. Timer) -> backlight turns off, but the Timer's countdown keeps running underneath (open it again or check after waking that time has kept passing, not frozen).
- Wake via encoder rotation instead of touch -> screen turns on, and the rotation that woke it does NOT also change the currently-displayed value (e.g. FAN's speed % is unchanged immediately after wake).
- Wake via encoder button press -> screen turns on, and does NOT also quit the currently open app.
- Tap the phone RFID card while the screen is asleep -> lights/fan still turn off (RFID_SERVICE runs independently of this loop, regression check).
