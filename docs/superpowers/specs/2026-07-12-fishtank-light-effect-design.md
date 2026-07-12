# Fish Tank Light Effect Control — Design

## Context

The fish tank light app (`app_set_brightness`, launcher slot 3) currently controls brightness (encoder) and on/off (touch button) via Home Assistant. The light also supports 6 built-in effect presets (no RGB/color_temp — confirmed by the user) exposed by HA as the `effect` (current) and `effect_list` (available names) attributes on the light entity. This adds effect selection without hardcoding the 6 names — read `effect_list` dynamically from HA so it keeps working if the light's supported effects ever change.

## Scope decisions

- **No RGB/color_temp control** — the light only supports named effect presets, not arbitrary color.
- **Effect names fetched dynamically** from HA's `effect_list` attribute, not hardcoded.
- **Single encoder, mode-switched by touch**: a new "MODE" button toggles what the encoder adjusts (BRIGHTNESS ⇄ EFFECT), replacing the single centered ON/OFF button with two side-by-side buttons (MODE, ON/OFF) in the same footprint.

## `ha_client` additions

```cpp
struct LightState
{
    bool ok = false;
    bool is_on = false;
    int brightness_pct = 0;
    std::string effect;                    // currently active effect name (may be empty)
    std::vector<std::string> effect_list;  // available effect names (may be empty if unsupported)
};

bool set_light_effect(const char* base_url, const char* token,
                       const char* entity_id, const char* effect_name);
```

`get_light_state` (existing function, modified) additionally reads `attributes.effect` (string, optional) and `attributes.effect_list` (array of strings, optional — absent entirely on lights without effect support, which just means `effect_list` stays empty and the MODE button has nothing to cycle through, handled gracefully rather than erroring).

`set_light_effect`: `POST /api/services/light/turn_on` with `{"entity_id": ..., "effect": "<name>"}` — same shape as `set_light_brightness`, different field.

## `app_set_brightness` changes

**New data fields:**
```cpp
enum class ControlMode { BRIGHTNESS, EFFECT };

ControlMode control_mode = ControlMode::BRIGHTNESS;
std::vector<std::string> effect_list;
int effect_index = 0;
bool effect_dirty = false;
uint32_t last_effect_change_ms = 0;
```

**Encoder behavior** (`_handle_encoder`):
- `ControlMode::BRIGHTNESS`: unchanged — ±5%, debounced 400ms, `set_light_brightness`.
- `ControlMode::EFFECT`: rotate moves `effect_index` by ±1, wrapping within `[0, effect_list.size())` (no-op if `effect_list` is empty), debounced 400ms (same pattern, even though effect changes are discrete — protects against flooding HA if the user spins the encoder quickly through several effects), `set_light_effect(effect_list[effect_index])`.

**New touch button "MODE"** (next to the existing ON/OFF button, not replacing it): tapping toggles `control_mode` between `BRIGHTNESS` and `EFFECT`. If `effect_list` is empty when the user tries to switch to `EFFECT` mode, stay in `BRIGHTNESS` mode instead (nothing to select) — this can't happen for the confirmed fish tank light, which does support effects, but the code shouldn't crash or show a blank state for a light that doesn't.

**Bubble content** now depends on `control_mode`:
- `BRIGHTNESS`: big text = `"<pct>%"` (unchanged from before)
- `EFFECT`: big text = current effect name (`effect_list[effect_index]`, or `"(no effects)"` if the list is empty)
- The "FISH LIGHT" label below stays as-is in both modes; a small mode indicator is added (e.g. small text above the big value: "BRIGHTNESS" or "EFFECT") so it's clear what the encoder currently adjusts.

**Layout**: bubble geometry unchanged (240×140 centered at 120,120). Below the bubble, two buttons side by side instead of one centered ON/OFF button:
- MODE button, left (e.g. centered x=70)
- ON/OFF button, right (e.g. centered x=170)
Both at the same y/height as the current single button (y=202, height=24), sized via `textWidth()` like every other button built this session (not guessed widths).

**On `onCreate`/`_refresh_state`**: after `get_light_state`, also populate `effect_list` and find the index of the current `effect` within it (falls back to index 0 if the current effect string isn't found in the list, e.g. right after a fresh boot before any effect has been explicitly set).

## Testing / verification

Same posture as every app this session: `idf.py build` per task, then flash + manual checklist:
- MODE button toggles between brightness and effect display/control
- In effect mode, rotating the encoder actually changes the fish tank light's effect within ~400ms of stopping, cycling through all 6 without erroring at the wrap-around boundary
- In brightness mode, behavior is unchanged from before this feature
- ON/OFF button still works, independent of which mode is active
