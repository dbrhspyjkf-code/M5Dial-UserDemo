# M5Dial Timer App — Design

## Context

M5Dial-UserDemo is an ESP-IDF + LovyanGFX app suite (Mooncake-style `APP_BASE` apps launched from a circular 8-icon `Launcher`). We're adding a countdown timer app, using the timer feature in the `smart-home-button` (ESPHome/LVGL) project as a reference for behavior, but re-implemented natively in C++ against this codebase's own conventions rather than ported 1:1.

Reference: `~/Projects/smart-home-button/src/pages/timer.yaml` + `src/pages/main.yaml` (physical-button/encoder routing).

## Scope

- Countdown timer only, MM:SS (no hours field — simpler touch zones, 99:59 max is enough for the use case).
- No persistence across app restart/quit — matches reference behavior (resets to last-edited duration).
- No Home Assistant / network dependency (M5Dial-UserDemo is fully local).

## Deviations from the reference (deliberate)

The reference (smart-home-button) uses "single click = start/pause, double click = quit" on the physical encoder button, distinguished via ESPHome `on_multi_click` timing windows. **We are not porting this.** Every existing app in M5Dial-UserDemo uses a single global convention: encoder button press-and-release = immediately quit the app, no click-count disambiguation. Introducing double-click detection here would make this one app behave differently from every sibling app. Instead:

- All state transitions (select field, start/pause/resume/reset) are driven by **touch**, using the existing `hal.tp` touchpad already present on every app.
- Encoder rotate still adjusts the currently selected field's value (this matches the reference's use of the encoder for value adjustment).
- Encoder button press-and-release still means "quit", unconditionally, same as every other app.

Visual style also deliberately matches the existing M5Dial app aesthetic (white rounded bubble on a themed circular background, like `Set_Brightness`) rather than the reference's dark background + circular progress arc — simpler to implement and consistent with sibling apps. No color-transition progress feedback (green/orange/red) — out of scope for this pass.

## File structure

```
main/apps/app_timer/
├── app_timer.h
├── app_timer.cpp
└── gui/
    ├── gui_timer.h
    └── gui_timer.cpp
```

Namespace `TIMER` (struct `TIMER::Data_t`), class `AppTimer : public MOONCAKE::APP_BASE`, GUI class `GUI_Timer : public GUI_Base`. Naming mirrors `SET_BRIGHTNESS` / `Set_Brightness` / `GUI_SetBrightness`.

## Launcher integration

- `main/apps/launcher/launcher.h`: add `#include "../app_timer/app_timer.h"`
- `main/apps/launcher/launcher.cpp`: add `case 8: app_ptr = new MOONCAKE::USER_APP::AppTimer; break;` in `_app_open_callback`
- `main/apps/launcher/launcher_render_callback.hpp`:
  - `ICON_NUM` 8 → 9
  - append one entry to `icon_color_list` (pick an unused accent color), `icon_tag_list` (`"TIMER"`, `"SET"`), `icon_pic_list` (`image_data_icon_timer`)
- The circular menu grid in `launcher.cpp::_menu_init()` already lays out 10 positions (`n = 10`) — bumping `ICON_NUM` to 9 fits inside the existing grid without layout changes, it just fills one of the two currently-empty slots.

## New icon asset

`main/apps/launcher/launcher_icons/icon_timer.h` — 42×42 RGB565 array, same format as existing icons (`icon_data_icon_timer[1764]`, generated from a simple stopwatch/hourglass glyph, black = transparent background per the existing `pushRotateZoom(..., TFT_BLACK)` convention). Registered via `#include "icon_timer.h"` in `launcher_icons.h`.

## Data model (`TIMER::Data_t`)

```cpp
enum class Field { MINUTES, SECONDS };
enum class State { EDIT, RUNNING, PAUSED, DONE };

struct Data_t {
    HAL::HAL* hal = nullptr;

    State state = State::EDIT;
    Field selected_field = Field::SECONDS;

    int set_minutes = 5;      // last-edited duration, restored on reset
    int set_seconds = 0;

    int remaining_seconds = 300;

    uint32_t last_tick_ms = 0;   // for the 1s countdown tick
    uint32_t last_blink_ms = 0;  // for DONE-state blink
    bool blink_on = true;

    int delta_time = 0;               // encoder acceleration, mirrors Set_Brightness
    uint32_t scroll_speed_time_count = 0;
};
```

## Interaction / state machine

**Layout** (matches `Set_Brightness`'s bubble style):
- Themed circular background (per-app `theme_color`, same as other apps get from `icon_list[selectedNum].color`)
- White rounded bubble, center: big "MM:SS" text — left half (MM) and right half (SS) are separate touch-hit-test zones
- Selected half gets a visual highlight (underline or outline in `_theme_color`)
- "TIMER" label below the digits
- Pill button below the bubble, label reflects state: `START` (EDIT) / `PAUSE` (RUNNING) / `RESUME` (PAUSED) / `RESET` (DONE)
- Bottom: `<` quit indicator (existing `_draw_quit_button()` from `GUI_Base`, unchanged)

**States:**

1. **EDIT** (default / after reset): tap left half of digits → `selected_field = MINUTES`; tap right half → `SECONDS`. Encoder rotate adjusts the selected field (seconds wraps 0–59, minutes clamps 0–99, using the same delta-time-based acceleration curve already in `Set_Brightness::onRunning`). Tap pill (`START`) → compute `remaining_seconds = set_minutes*60 + set_seconds`; if `> 0`, go to **RUNNING**.
2. **RUNNING**: every 1000ms (checked via `millis() - last_tick_ms`, no RTOS timer needed), `remaining_seconds -= 1`; at 0 → go to **DONE** (fire buzzer once). Tap pill (`PAUSE`) → **PAUSED**. Digits are read-only in this state (no field highlight, taps on digits ignored).
3. **PAUSED**: on entry, decompose `remaining_seconds` into `set_minutes = remaining_seconds/60`, `set_seconds = remaining_seconds%60` so the same MM/SS fields used in EDIT become editable. Same digit-tap-to-select + encoder-adjust as EDIT, operating on `set_minutes`/`set_seconds`. Tap pill (`RESUME`) → recompute `remaining_seconds = set_minutes*60 + set_seconds`, go to **RUNNING** (resets `last_tick_ms` so the next second isn't short).
4. **DONE**: `hal.buzz.tone(...)` fires once on entry. Digits blink (toggle visibility/dim every ~500ms via `last_blink_ms`) until acknowledged. Tap pill (`RESET`) → restore `set_minutes`/`set_seconds` (the values from before the countdown started), go to **EDIT**.
5. **Quit** (any state): encoder button press-and-release → `destroyApp()`, unconditionally — identical to every other app in this codebase. No state is persisted.

## Testing / verification

No unit test harness exists in this codebase (embedded ESP-IDF app apps are verified by flashing + manual interaction, same as every other app here). Verification plan:
- `idf.py build` succeeds with the new app compiled in
- Flash to device, open Timer from launcher (9th icon)
- Manually verify: field selection via tap, encoder adjusts correct field, start/pause/resume/reset cycle, buzzer fires at 00:00, quit via encoder button from every state
