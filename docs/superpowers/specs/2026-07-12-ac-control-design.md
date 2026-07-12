# AC Control (TEMP CTRL → AC) Design

## Goal

Repurpose `app_temp_demo` (currently `VideoShit`/`GUI_VideoShit`, a demo screen) into a Home Assistant climate controller for the master bedroom air conditioner `climate.lumi_cn_74788630_v3`. Same repurposing pattern as the earlier LCD-TEST→TV, BRIGHTNESS-SET→fish-tank-light, and RTC-TEST→FAN features in this project.

## Launcher

- Tag renamed `"TEMP CTRL","DEMO"` → `"AC","CTRL"`.
- Icon unchanged — the existing thermometer icon (`icon_temp.h`) already fits a temperature-control app; no new icon needed.

## HA entity & capabilities

`climate.lumi_cn_74788630_v3` (standard HA `climate` domain):
- top-level `state`: the current HVAC mode itself (e.g. `"cool"`, `"heat"`, `"off"`, `"fan_only"`, `"dry"`, `"auto"`) — same "state IS the value" shape already handled for the `number` domain in the TV feature.
- `attributes.current_temperature`: float, room's current temperature (read-only, display only)
- `attributes.temperature`: float, target temperature
- `attributes.hvac_modes`: array of strings — the modes this entity actually supports, read dynamically (never hardcoded), same convention as the fish-tank light's `effect_list`
- `attributes.min_temp` / `attributes.max_temp`: float, target-temperature range

Services: `climate.set_temperature` (`{entity_id, temperature}`), `climate.set_hvac_mode` (`{entity_id, hvac_mode}`). No separate turn_on/turn_off service in the climate domain — power off is `set_hvac_mode` to `"off"`; power on restores the last non-off mode.

## HA_CLIENT additions

New in `main/apps/utilities/ha_client/ha_client.h`/`.cpp`:
- `ClimateState{ok, hvac_mode, target_temp, current_temp, hvac_modes, min_temp, max_temp}` + `get_climate_state(base_url, token, entity_id)`.
- `set_climate_temperature(base_url, token, entity_id, float temperature)` → `climate.set_temperature`.
- `set_climate_hvac_mode(base_url, token, entity_id, const char* hvac_mode)` → `climate.set_hvac_mode`.

## App behavior

Same `CONNECTING`/`CONTROLLING`/`ERROR` state machine, persistent-WiFi convention (no `WIFI_CONNECT::disconnect()` in `onDestroy()`), and "render immediately with default/last-known values, no blank flash or 'Connecting...' screen" convention established for the other 4 HA apps.

- **Encoder rotate**: adjusts target temperature ±0.5°C per detent, clamped to `[min_temp, max_temp]`, debounced 400ms before calling `set_climate_temperature` (same debounce pattern as brightness/volume/fan-speed).
- **Touch POWER button**: toggles power.
  - Turning off: remember the current `hvac_mode` as `last_active_mode`, then call `set_climate_hvac_mode(entity_id, "off")`.
  - Turning on: call `set_climate_hvac_mode(entity_id, last_active_mode)` if we have one from this session, otherwise the first entry in `hvac_modes` that isn't `"off"`.
- **Touch MODE button**: cycles to the next mode in `hvac_modes` (skipping `"off"` — that's what the POWER button is for), calls `set_climate_hvac_mode`, then refreshes state. Disabled/no-op while powered off (MODE button only meaningful when the unit is on).
- **Display**: target temperature large in the middle (e.g. "26.0°C"), current mode name as a small label, POWER and MODE buttons at the bottom — same layout as the fish-tank-light app (brightness value + MODE/ON buttons).

## Config

New `main/apps/app_temp_demo/ac_config.h` (gitignored) + `ac_config.example.h` (committed template) — same WiFi SSID/password/HA base URL/token as the other 4 HA apps, plus `AC_ENTITY_ID = "climate.lumi_cn_74788630_v3"`.

## Out of scope

- No fan-speed control (`fan_mode`) — explicitly excluded per this feature's scope.
- No swing/louver control.
- No renaming of the `VideoShit`/`GUI_VideoShit` C++ class names — keeps the launcher wiring (`case` index, `getGui()`) untouched, same as how `LCD_Test`/`RTC_Test` kept their class names through their own repurposes.
