# Fan Control (RTC TEST → FAN) Design

## Goal

Repurpose the `app_rtc_test`/`gui_rtc_test` app (currently a clock display, launcher tag "RTC"/"TIME") into a Home Assistant fan controller for the floor fan `fan.dmaker_cn_740412216_p5c_s_2_fan`. Same pattern as the earlier LCD-TEST→TV and BRIGHTNESS-SET→fish-tank-light repurposes in this project.

## Launcher

- Tag renamed `"RTC","TIME"` → `"FAN","CTRL"` (icon graphic replaced with the user-supplied fan icon, not left as the old clock icon).
- New icon `icon_fan.h`/`icon_fan_preview.png` generated from `~/Desktop/export_1.png` — a white-line-art fan-blade glyph on a baked-in checkerboard "fake transparency" background (not real alpha). Recolored to accent `0x38D9FF` (cyan) via brightness threshold (white lines → accent, checkerboard → black/transparent-key), resized to 42×42 with LANCZOS, hard circular cutoff applied at the final 42×42 size, byte-swapped RGB565 — same pipeline as every prior icon in this project.

## HA entity & capabilities

`fan.dmaker_cn_740412216_p5c_s_2_fan` exposes (standard HA `fan` domain):
- top-level `state`: `"on"`/`"off"`
- `attributes.percentage`: int 0-100 (speed)
- `attributes.oscillating`: bool (swing/摇头)

Services: `fan.turn_on`, `fan.turn_off` (power), `fan.set_percentage` (`{entity_id, percentage}`), `fan.oscillate` (`{entity_id, oscillating}`).

## HA_CLIENT additions

New in `main/apps/utilities/ha_client/ha_client.h`/`.cpp`:
- `FanState{ok, is_on, percentage, oscillating}` + `get_fan_state(base_url, token, entity_id)` — `GET /api/states/{entity_id}`, reads top-level `state` for on/off and `attributes.percentage`/`attributes.oscillating`.
- `set_fan_power(base_url, token, entity_id, bool on)` → `fan.turn_on`/`fan.turn_off`.
- `set_fan_percentage(base_url, token, entity_id, int percentage)` → `fan.set_percentage` with `{entity_id, percentage}`.
- `set_fan_oscillating(base_url, token, entity_id, bool oscillating)` → `fan.oscillate` with `{entity_id, oscillating}`.

## App behavior

Same state machine as fish-tank-light (`CONNECTING` → `CONTROLLING` / `ERROR`, tap-to-retry on error), same persistent-WiFi convention (connect() is a no-op since boot already connected it, no disconnect() in onDestroy — same as the other 3 HA apps after the RFID feature).

- **Encoder rotate**: adjusts speed ±10% per detent (0-100, clamped), debounced 400ms before calling `set_fan_percentage` (same debounce pattern as brightness/volume).
- **Touch POWER button**: toggles fan on/off immediately, calls `set_fan_power`, then refreshes state.
- **Touch SWING button**: toggles oscillation immediately, calls `set_fan_oscillating`, then refreshes state.
- **Display**: "FAN" title near top, speed percentage large in the middle, POWER and SWING buttons at the bottom (replacing fish-tank's MODE/ON row), oscillation state reflected in the SWING button's highlight state (like effect list toggle vs plain button).

## Config

New `main/apps/app_rtc_test/fan_config.h` (gitignored) + `fan_config.example.h` (committed template) — same WiFi SSID/password/HA base URL/token as the other 3 HA apps, plus `FAN_ENTITY_ID = "fan.dmaker_cn_740412216_p5c_s_2_fan"`.

## Out of scope

- No preset-mode support (dmaker fan's natural-wind/preset modes aren't exposed here — plain percentage speed only, matching what the user asked for).
- No renaming of the `RTC_Test`/`GUI_RTC_Test` C++ class names — keeps the launcher wiring (`case` index, `getGui()`) untouched, same as how `LCD_Test` kept its class name through the TV repurpose.
