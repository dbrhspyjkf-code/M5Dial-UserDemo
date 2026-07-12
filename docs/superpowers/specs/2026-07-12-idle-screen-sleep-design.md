# Idle Screen Sleep Design

## Goal

After 5 minutes with no touch/encoder/button activity, turn the screen backlight off to save power. The next touch, encoder rotation, or encoder-button press wakes the screen back up. Whatever app is open keeps its own logic running in the background the whole time (timers keep counting, Sonos keeps polling) — only the backlight goes dark.

## Where this hooks in

This project's execution model is single-task/blocking: exactly one `while(1) { xxx.onRunning(); }` loop is ever active — either `main.cpp`'s loop driving `Launcher`, or `Launcher::_simple_app_manager`'s loop driving whichever app is currently open. Every app funnels through `_simple_app_manager`, so hooking idle-detection into just these 2 loop sites covers the Launcher and all 10 apps without touching any individual app file.

Each loop iteration calls `IDLE_SCREEN::tick(hal)` before calling `onRunning()`:

```cpp
while (1)
{
    if (!IDLE_SCREEN::tick(hal))
    {
        app->onRunning();  // or app_launcher.onRunning() in main.cpp
    }
    // ...existing loop body (isGoingDestroy check, etc.) unchanged
}
```

`tick()` returns `true` only for the exact cycle where it detects a wake-from-sleep gesture, so `onRunning()` is skipped for that one cycle only. Every other cycle — whether the screen is on, or asleep, or just handling ordinary activity — calls `onRunning()` normally, so app-internal timers/polling are never paused.

## Activity detection

"Activity" (resets the 5-minute idle timer) is:
- `hal->tp.isTouched()` — touch
- `hal->encoder.getCount()` changing since the last check — encoder rotation
- `hal->encoder.btn.read()` reading pressed — encoder button

Encoder rotation is tracked via `IDLE_SCREEN`'s own private copy of the last-seen `getCount()` value, completely separate from `ESP32Encoder::wasMoved()`'s internal `_last_count` state that every app already uses for its own rotation handling. This is deliberate: `wasMoved()` has a side effect (it updates its internal `_last_count`), so if `IDLE_SCREEN` called it on every tick to check for activity, it would silently consume rotation events before the currently-open app's own `onRunning()` got a chance to see them — breaking every app's encoder-driven control (volume, brightness, speed, timer values, etc.). Reading `getCount()` directly has no side effects, so `IDLE_SCREEN` can observe rotation without interfering.

## Sleep / wake behavior

**Going to sleep**: when no activity has been seen for 5 minutes (`millis() - last_activity_ms > 5*60*1000`) and the screen is currently on, call `hal->display.setBrightness(0)` and mark the screen off. No screen content changes — the last frame stays in the framebuffer, just invisible with the backlight off.

**Waking up**: when activity is detected while the screen is off:
1. Restore the backlight: `hal->display.setBrightness(128)` (matches the steady-state brightness `hal.cpp`'s boot animation already ramps up to).
2. Fully absorb the gesture that caused the wake, so it can't also trigger whatever action it would normally perform (same as a phone lock screen — the first tap just wakes it):
   - If it was a touch: block until `tp.isTouched()` goes false (touch released), same "hold until release" idiom already used throughout every app's own touch handlers.
   - If it was the encoder button: block until `btn.read()` reads released, same idiom used everywhere else for the quit gesture.
   - If it was encoder rotation: call `hal->encoder.wasMoved(true)` once to absorb the pending rotation into `ESP32Encoder`'s own internal tracking, so the currently-open app's very next `wasMoved()` call correctly sees no leftover movement from before the wake.
3. Return `true` (consumed) for this one cycle only. The very next loop iteration resumes calling `onRunning()` normally, and any further touches/rotations are treated as genuine input.

## File

New `main/apps/utilities/idle_screen/idle_screen.h` / `.cpp`, namespace `IDLE_SCREEN`, one function: `bool tick(HAL::HAL* hal);`. No config needed — the 5-minute timeout and brightness value (128) are compile-time constants in the `.cpp`, matching how other simple constants (debounce intervals, etc.) are already handled in this codebase.

## Out of scope

- No configurable timeout (hardcoded 5 minutes, matching the exact request).
- No dimming/fade transition — brightness is either 128 (on) or 0 (off), matching the codebase's existing on/off brightness convention.
- No changes to any of the 10 individual app files, `HAL`, or `ESP32Encoder` — purely additive.
