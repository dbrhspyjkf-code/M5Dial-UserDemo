# TV Control via Home Assistant — Design

## Context

`app_lcd_test` (launcher slot 0, tagged "LCD"/"TEST") currently just cycles through LCD color-test pages — no networking. This repurposes it to control a TV (Xiaomi, exposed to Home Assistant as `switch.xiaomi_esprh1_0bc4_is_on` and `number.xiaomi_esprh1_0bc4_volume`) via `wifi_connect_wrap`/`ha_client`, following the exact pattern already used for the Sonos and fish-tank-light apps.

## Scope decisions

- **Power is inverted through a "sound bar mode" switch**: `switch.xiaomi_esprh1_0bc4_is_on` = ON means the TV is OFF (sound-bar-only mode), and OFF means the TV is ON. The app's UI shows the intuitive TV on/off state; the inversion is an implementation detail hidden inside the app (`tv_on = !switch.is_on`), never exposed as "sound bar mode" language in the UI.
- **Volume via `number.xiaomi_esprh1_0bc4_volume`**, using HA's `number` domain (`number.set_value` service), not `media_player`.
- **Volume range read dynamically** from the `number` entity's `min`/`max` attributes, not hardcoded to 0–100 — same reasoning as reading the fish tank light's `effect_list` dynamically instead of hardcoding effect names.
- **No mode switch needed** — unlike the fish tank light (brightness + effects), there's only one adjustable value (volume), so the encoder always adjusts it; no MODE button.

## `ha_client` additions

```cpp
struct SwitchState
{
    bool ok = false;
    bool is_on = false;
};

SwitchState get_switch_state(const char* base_url, const char* token, const char* entity_id);
// Setting a switch reuses the existing generic call_service("switch", "turn_on"/"turn_off", entity_id) — no new POST logic needed.

struct NumberState
{
    bool ok = false;
    float value = 0.0f;
    float min = 0.0f;
    float max = 100.0f;
};

NumberState get_number_state(const char* base_url, const char* token, const char* entity_id);
bool set_number_value(const char* base_url, const char* token, const char* entity_id, float value);
```

- `get_switch_state`: `GET /api/states/{entity_id}`, parse top-level `"state"` (`"on"`/`"off"`) — same shape as `get_light_state`'s power parsing, no attributes needed.
- `get_number_state`: `GET /api/states/{entity_id}`. For the `number` domain, HA's top-level `"state"` field is itself the current value, as a numeric string (not `"on"`/`"off"`) — parse with `atof()`/`strtof()`. `attributes.min` and `attributes.max` are numbers, parsed like any other numeric attribute.
- `set_number_value`: `POST /api/services/number/set_value` with `{"entity_id": ..., "value": N}`.

## `app_lcd_test` — full internal replacement

**Files unchanged in location, contents replaced** (same treatment as `app_set_brightness`):
- `main/apps/app_lcd_test/app_lcd_test.h` / `.cpp`
- `main/apps/app_lcd_test/gui/gui_lcd_test.h` / `.cpp`

**Config**: new `main/apps/app_lcd_test/tv_config.h` (gitignored) + `.example.h` (committed), same WiFi/HA credentials already used by the Sonos and fish-tank-light apps, plus:
```cpp
#define TV_SWITCH_ENTITY_ID "switch.xiaomi_esprh1_0bc4_is_on"
#define TV_VOLUME_ENTITY_ID "number.xiaomi_esprh1_0bc4_volume"
```

**State machine** (same shape as the other two HA-backed apps):
```cpp
enum class State { CONNECTING, CONTROLLING, ERROR };
```
1. **CONNECTING**: `wifi_connect(...)`, then `get_switch_state()` + `get_number_state()`. Either failing → **ERROR**.
2. **CONTROLLING**: encoder rotate adjusts a locally-tracked `volume` by a step (`(max - min) / 20`, so a full sweep of the range takes ~20 detents regardless of the entity's actual scale), clamped to `[min, max]`, debounced 400ms before `set_number_value()` — identical debounce pattern to the brightness/volume handling in the other two apps. Touch on the power button toggles the displayed `tv_on` and calls `set_switch_state` — implemented as `call_service("switch", tv_on ? "turn_off" : "turn_on", TV_SWITCH_ENTITY_ID)` (turning the TV on means turning the sound-bar-mode switch *off*), then re-fetches state so the display stays consistent with reality.
3. **ERROR**: same as the other two apps — reason + "TAP TO RETRY", tap retries.
4. **Quit** (any state): encoder button press-and-release → `wifi_disconnect()` + `destroyApp()`, unconditional.

**Layout**: same bubble style as the fish-tank-light app's brightness-only view (before the effect feature was added) — bubble with a "TV" label, big number for volume, single centered ON/OFF button below showing the TV's (already-inverted) power state. No MODE button — only one adjustable value.

## Testing / verification

Same posture as every HA-backed app built this session: `idf.py build` per task, then flash + manual checklist:
- Opening the app shows "Connecting...", then the volume bubble
- Encoder rotation changes the TV's actual volume within ~400ms of stopping, doesn't flood HA with a request per detent
- Tapping the ON/OFF button actually turns the TV on/off (verified against the physical TV, not just the switch entity's state in HA) — specifically confirm the inversion is correct: tapping when displayed as "OFF" turns the TV on and displays "ON" afterward
- WiFi/HA failure shows the error screen with retry, not a hang
- Quit from every state returns to the launcher
