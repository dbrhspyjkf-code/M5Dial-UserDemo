# Sonos Control via Home Assistant — Design

## Context

M5Dial-UserDemo currently has no networking beyond a one-shot WiFi scan (`app_wifi_scan`), no HTTP client, no JSON library, and no runtime text-input UI (LVGL is disabled — `#define LVGL_ENABLE 0` in `main/hal/hal.h`). This feature adds a new app that connects to WiFi, talks to a local Home Assistant instance over its REST API, and controls a Sonos speaker (`media_player.ke_ting`): play/pause, next/previous track, volume (via encoder), and shows the currently-playing track/artist.

## Scope decisions (from clarifying questions)

- **Config is compile-time, not runtime.** WiFi SSID/password and the HA base URL/token are hardcoded macros, matching the existing `FACTORY_TEST_WIFI_SSID` pattern in `wifi_factory_test.c`. No on-device text input UI is built (would require enabling LVGL and building a keyboard — out of scope for this feature).
- **HA REST API over plain HTTP** (local network, no TLS) using a long-lived access token in the `Authorization: Bearer` header — not the WebSocket API (not worth the connection/auth-handshake complexity for a device that only needs occasional polling + fire-and-forget commands).
- **Poll every 3 seconds** for now-playing state — matches the polling cadence already used elsewhere in similar projects (e.g. `smart-home-button`'s sensor polling).
- **WiFi is connected only while this app is open**, and disconnected on exit — matches `app_wifi_scan`'s existing lifecycle convention (no persistent background WiFi).

## Secrets handling

The user provided a real WiFi password and a real HA long-lived access token in chat (not reproduced here). These go into `main/apps/app_sonos/sonos_config.h`, which is **gitignored** — never committed, not even in this spec. A committed `main/apps/app_sonos/sonos_config.example.h` holds the same `#define`s with placeholder values, so the repo stays buildable-by-example without leaking credentials, mirroring the `secrets.yaml` / `secrets.example.yaml` pattern already used in the `smart-home-button` project.

```cpp
// sonos_config.h (gitignored, real values — created manually, not by this plan,
// and never committed or pasted into any tracked file)
#pragma once
#define SONOS_WIFI_SSID       "<real SSID>"
#define SONOS_WIFI_PASSWORD   "<real password>"
#define SONOS_HA_BASE_URL     "http://<HA host/IP>:8123"   // user fills in the actual host
#define SONOS_HA_TOKEN        "<real long-lived access token>"
#define SONOS_ENTITY_ID       "media_player.ke_ting"
```

`main/apps/app_sonos/sonos_config.example.h` (committed) has the same four `#define`s with obvious placeholder strings (`"YOUR_WIFI_SSID"`, etc.) and a comment: copy to `sonos_config.h` and fill in.

**The user needs to fill in `SONOS_HA_BASE_URL`'s host/IP** — it wasn't provided in chat (only the token, entity ID, and WiFi credentials were).

## Architecture

```
main/apps/utilities/wifi_connect_wrap/
├── wifi_connect_wrap.h
└── wifi_connect_wrap.cpp
    bool wifi_connect(const char* ssid, const char* password, uint32_t timeout_ms);
    void wifi_disconnect();

main/apps/utilities/ha_client/
├── ha_client.h
└── ha_client.cpp
    struct MediaPlayerState { std::string state; std::string title; std::string artist; float volume; bool ok; };
    MediaPlayerState ha_get_state(const char* base_url, const char* token, const char* entity_id);
    bool ha_call_service(const char* base_url, const char* token, const char* domain,
                          const char* service, const char* entity_id);
    bool ha_set_volume(const char* base_url, const char* token, const char* entity_id, float volume);

main/apps/app_sonos/
├── app_sonos.h / app_sonos.cpp        (state machine, touch/encoder handling)
├── sonos_config.example.h             (committed template)
├── sonos_config.h                     (gitignored, real values)
└── gui/
    ├── gui_sonos.h / gui_sonos.cpp     (renderer)
```

- `wifi_connect_wrap` is a thin, blocking wrapper around the existing `esp_wifi_set_config` + event-group-wait pattern already in `wifi_factory_test.c`'s `wifi_init_sta()`, refactored into a reusable function instead of a one-off with hardcoded macros baked in.
- `ha_client` wraps `esp_http_client` (IDF built-in, declared via `REQUIRES esp_http_client` in `main/CMakeLists.txt`) and `cJSON` (IDF built-in, `REQUIRES cjson`) for the two request shapes HA needs: `GET /api/states/{entity_id}` and `POST /api/services/{domain}/{service}`.
- Both utilities are dependency-free of `AppSonos` (take plain strings/structs in, return plain structs out) so they're independently readable/testable in isolation, following the same GUI/App separation already used by every other app in this codebase.

## State machine (`AppSonos`)

```
enum class State { CONNECTING, POLLING, ERROR };
```

1. **CONNECTING** (on `onCreate`): call `wifi_connect(SONOS_WIFI_SSID, SONOS_WIFI_PASSWORD, 8000)`. On success, do one immediate `ha_get_state()` call and move to **POLLING** (or **ERROR** if that first HA call fails — WiFi connected but HA unreachable/token bad). On WiFi connect timeout, move to **ERROR**.
2. **POLLING**: every 3000ms (millis()-based, same pattern as `AppTimer`'s tick handling), call `ha_get_state()` and update the displayed title/artist/state/volume. Touch handling:
   - Tap **prev** → `ha_call_service(..., "media_player", "media_previous_track", entity_id)`
   - Tap **play/pause** → `ha_call_service(..., "media_player", "media_play_pause", entity_id)`
   - Tap **next** → `ha_call_service(..., "media_player", "media_next_track", entity_id)`
   - Encoder rotate → adjust a locally-tracked volume value by ±0.05 per detent (clamped 0.0–1.0), **debounced**: only actually calls `ha_set_volume()` after 400ms of no further rotation (avoids flooding HA with a request per encoder tick — mirrors why `AppTimer`'s encoder handling doesn't fire on every micro-step either, though there it's not networked so there was no flooding risk; here there is).
3. **ERROR** (WiFi or HA failure at any point — including a POLLING-state HA call failing 3 times in a row): show the error reason (`"WiFi failed"` / `"HA unreachable"`) and a "TAP TO RETRY" hint. Tap anywhere → back to **CONNECTING**.
4. **Quit** (any state): encoder button press-and-release → `wifi_disconnect()`, then `destroyApp()` — same unconditional-quit convention as every other app.

## Layout (matches `AppTimer`'s "flat on themed background" style, no white bubble)

- Top: existing `_draw_top_icon()`
- "SONOS" label (small, white, like Timer's "TIMER" label)
- Track title (white, size 2, truncated with `...` if it doesn't fit — `textWidth()`-based truncation like the Timer pill button's width computation, not a guessed character limit)
- Artist (dimmer gray, size 1, below title)
- Volume readout (e.g. "VOL 42%") below artist
- Three touch buttons in a row: `|<<` `▶/❚❚` `>>|` — same white-pill style as Timer's pill button, each sized via `textWidth()` + padding like Timer's pill, not guessed widths
- Bottom: existing quit `<` indicator

## New icon asset

10th launcher icon, in the same style as the Timer icon (recolored user-supplied glyph + circular hard-cutoff mask + byte-swapped RGB565, matching the process worked out for the Timer icon). Needs a Sonos/speaker-shaped source glyph — reuse the same recoloring script, pointed at a new source image (to be supplied the same way `timer.png` was: user drops a flat white-on-purple icon PNG on Desktop, tells the path).

## Error handling specifics

- `esp_http_client` calls use a fixed connect+read timeout (5000ms) so a dead network doesn't hang the app indefinitely.
- HA's response body length isn't known ahead of time — read into a fixed-size buffer (e.g. 2048 bytes, generous for a media_player state's typical JSON) rather than dynamically sizing, matching this codebase's general preference for fixed buffers over dynamic allocation in app code (`char mm_buf[3]` etc. throughout).
- `cJSON_Parse` failure or missing expected keys (`attributes.media_title` etc.) → treat as "ok=false" in `MediaPlayerState`, same as an HTTP-level failure, so `AppSonos` only has one failure path to handle, not two.

## Testing / verification

Same as `AppTimer`: no unit test harness in this codebase. Verification is `idf.py build` per task (catches compile errors), then a final flash + manual checklist:
- App connects to WiFi and shows track/artist within a few seconds of opening
- Play/pause, next, previous all produce an audible/visible effect on the actual Sonos within one poll cycle
- Encoder rotation changes volume (audible), doesn't spam HA (check HA's logbook/history isn't full of one entry per encoder tick)
- Turning off WiFi (e.g. disabling the AP) mid-use surfaces the ERROR state instead of hanging
- Quit (encoder button) from CONNECTING, POLLING, and ERROR all return to the launcher
