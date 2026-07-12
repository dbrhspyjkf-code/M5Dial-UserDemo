# Fish Tank Light Brightness via Home Assistant — Design

## Context

`app_set_brightness` (launcher slot 3, tagged "BRIGHTNESS"/"SET") currently controls the M5Dial's own display backlight brightness — purely local, no networking (`main/apps/app_set_brightness/app_set_brightness.cpp`, `main/apps/app_set_brightness/gui/gui_set_brightness.cpp`). This repurposes it to instead control the brightness (and on/off) of the fish tank light in Home Assistant, entity `light.xiaomi_cn_931286672_m200_s_3_light` (same entity used in the M5Tab5 project's fish tank card).

This reuses the `wifi_connect_wrap` and `ha_client` utilities built for the Sonos app (`main/apps/utilities/wifi_connect_wrap/`, `main/apps/utilities/ha_client/`) — no new networking infrastructure needed, just two new `ha_client` functions for the `light` domain (different shape from `media_player`: HA's `light.turn_on` service takes a `brightness_pct` field, and state attributes have `brightness` as a 0–255 byte, not a 0–1 float like `media_player`'s `volume_level`).

## Scope decisions (from clarifying questions)

- **Entity confirmed:** `light.xiaomi_cn_931286672_m200_s_3_light`.
- **On/off control included** — a touch button, not just brightness.
- **Visual style unchanged**: keep the existing white-bubble-on-themed-background look (`Set_Brightness`'s current style), not the newer flat style used by `AppTimer`/`AppSonos`. Only the *content* changes (brightness now reflects the HA light's actual brightness, not the display's local backlight), plus a state machine for the new WiFi/HA dependency and a new on/off button.
- **Launcher untouched** — same icon, same "BRIGHTNESS"/"SET" tag, same slot 3. Only the app's internals change.

## Architecture

### `ha_client` additions (`main/apps/utilities/ha_client/ha_client.h` / `.cpp`)

```cpp
struct LightState
{
    bool ok = false;
    bool is_on = false;
    int brightness_pct = 0; // 0-100, converted from HA's 0-255 attributes.brightness
};

LightState get_light_state(const char* base_url, const char* token, const char* entity_id);

bool set_light_brightness(const char* base_url, const char* token,
                           const char* entity_id, int brightness_pct);

bool set_light_power(const char* base_url, const char* token,
                      const char* entity_id, bool on);
```

- `get_light_state`: `GET /api/states/{entity_id}`, parse top-level `"state"` (`"on"`/`"off"`) and `attributes.brightness` (0–255 int, convert to 0–100 via `(brightness * 100 + 127) / 255`; absent when off — treat as `brightness_pct = 0` in that case, matching how a light UI conventionally shows 0% when off).
- `set_light_brightness`: `POST /api/services/light/turn_on` with `{"entity_id": ..., "brightness_pct": N}`.
- `set_light_power`: `POST /api/services/light/turn_on` or `/api/services/light/turn_off` (choosing the service name based on `on`) with `{"entity_id": ...}` — reuses the existing generic `call_service()` helper directly, no new HTTP logic needed for this one.

These are pure additions; nothing existing in `ha_client` changes.

### `app_set_brightness` — full internal replacement

**Files unchanged in location, contents replaced:**
- `main/apps/app_set_brightness/app_set_brightness.h` / `.cpp`
- `main/apps/app_set_brightness/gui/gui_set_brightness.h` / `.cpp`

**State machine** (same shape as `AppSonos`'s):
```cpp
enum class State { CONNECTING, CONTROLLING, ERROR };
```
1. **CONNECTING** (`onCreate`): `wifi_connect(...)`, then one `get_light_state()` call. Failure at either step → **ERROR**.
2. **CONTROLLING**: encoder rotate adjusts a locally-tracked `brightness_pct` by ±5 (clamped 0–100), debounced 400ms (identical debounce pattern to `AppSonos`'s volume handling) before calling `set_light_brightness()`. Touch on the new on/off button toggles `light_on` and calls `set_light_power()` immediately (not debounced — it's a discrete action, not a continuous adjustment). No periodic polling loop is needed here (unlike Sonos's now-playing display, brightness doesn't change from outside as often, and the app already has an immediate local copy of the value it's setting) — but re-fetch state once whenever the on/off button is pressed, so the displayed brightness stays consistent with reality after a power toggle.
3. **ERROR**: same as `AppSonos` — show reason + "TAP TO RETRY", tapping anywhere re-attempts `onCreate()`.
4. **Quit** (any state): encoder button press-and-release → `wifi_disconnect()` + `destroyApp()` — unconditional, matches every app.

**Layout** (keeps the existing bubble geometry from `gui_set_brightness.cpp`: 240×140 bubble centered at 120,120, radius 36):
- Top icon (unchanged)
- Bubble, unchanged size/position
- Big number = `brightness_pct` (was the raw 0–255 backlight value; now 0–100 HA brightness percent), same position/size/color as before (`bubble.y - 56`, size 3, `_theme_color` text on white)
- Label below number changes from `"BRIGHTNESS"` to `"FISH LIGHT"` (same position, `bubble.y + 26`, size 1)
- New: on/off toggle button below the bubble (bubble bottom edge is at `120 + 70 = 190`), a small pill labeled `"ON"`/`"OFF"` reflecting current `light_on` state, sized via `textWidth()` like the Timer/Sonos buttons (not a guessed width)
- Quit `<` indicator unchanged

**CONNECTING/ERROR screens** reuse the same bubble (draw the status text where the brightness number normally goes) rather than introducing a different layout — keeps one consistent visual frame for the whole app instead of a jarring layout swap between states.

## Error handling

Same posture as `AppSonos`: `esp_http_client` 5-second timeout, fixed 2048-byte response buffer (already established in `ha_client.cpp`, reused as-is), missing/unparseable JSON treated as `ok = false` on `LightState`.

## Testing / verification

No unit test harness (same as every other app here). Verification: `idf.py build` per task, then flash + manual checklist:
- Opening the app (still icon slot 3 in the launcher) shows "Connecting...", then the brightness bubble
- Encoder rotation changes the fish tank light's actual brightness within ~400ms of stopping, doesn't flood HA with a request per detent
- Tapping the on/off button actually turns the fish tank light on/off, and the displayed brightness updates accordingly
- WiFi/HA failure shows the error screen with retry, not a hang
- Quit from every state returns to the launcher
