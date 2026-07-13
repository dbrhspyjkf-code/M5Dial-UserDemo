# Launcher Screensaver + NTP Time Sync Design

## Goal

1. When the Launcher's icon carousel sits idle for 30 seconds, show a full-screen clock/date/weather screensaver. Any touch, encoder rotation, or encoder button press dismisses it (absorbing that gesture, same "wake without acting" convention as `IDLE_SCREEN`) and returns to the normal carousel.
2. Sync the device's RTC to real internet time via NTP once at boot, so the screensaver's clock is actually accurate (the RTC is battery-backed but drifts/needs a real starting point).

## Relationship to IDLE_SCREEN

Separate and independent from `IDLE_SCREEN` (which turns the backlight off after 5 minutes of no activity, device-wide, covering the Launcher and all apps). The screensaver:
- Only activates while sitting on the Launcher's icon carousel (never while an app is open — apps have their own `onCreate()`/`onRunning()` loops with their own logic).
- Has its own, shorter 30-second idle timer, tracked independently of `IDLE_SCREEN`'s activity tracking.
- Still keeps the backlight on (it's a normal rendered screen) — `IDLE_SCREEN` keeps running underneath and will still turn the backlight off after its own 5-minute timeout, whether the screensaver or the carousel is currently showing.

## Screensaver activity detection

Same pattern established for `IDLE_SCREEN`: track encoder position via `hal->encoder.getCount()` directly (never `wasMoved()`, which has the side effect of consuming the rotation before the Launcher's own carousel-scroll logic sees it) plus `hal->tp.isTouched()` plus `hal->encoder.btn.read()`. This tracking is independent of `IDLE_SCREEN`'s own copy — each keeps its own "last seen" state, since they're unrelated timers watching the same physical inputs for different purposes.

## Screen content

Full-screen, black background, white text (matches the Launcher's own black background — no themed bubble, this isn't tied to any app's color):
- Large time, `HH:MM`
- Date + weekday below it
- Weather: temperature + condition text below that (e.g. "37°C 少云")

## Data sources

**Time/date**: read from the device's own PCF8563 RTC (`hal.rtc.getTime(tm&)`) — no network needed to display it once synced.

**Weather**: `GET http://192.168.1.200:8766/weather` (existing `hermes-mcp-xiaozhi` endpoint, default city 广州, no auth needed) — confirmed live:
```json
{"city": "广州", "temp_c": "37", "feels_like_c": "44", "humidity": "51",
 "condition_code": "116", "condition": "少云", "wind_kmph": "12", "ok": true}
```
Only `temp_c` and `condition` are used. Fetched once each time the screensaver activates (not continuously polled while showing — it's on-screen for at most 5 minutes before `IDLE_SCREEN` blanks the backlight anyway, and weather doesn't change that fast).

New `main/apps/utilities/weather_client/weather_client.h`/`.cpp`, one-shot GET (same non-persistent-connection reasoning as `STOCK_CLIENT` — low call frequency, small response, no need for `HA_CLIENT`'s persistent-connection machinery).

## NTP time sync at boot

`main/apps/utilities/ntp_sync/ntp_sync.h`/`.cpp`: `void sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms)`, using ESP-IDF's built-in SNTP client (`esp_sntp.h`, already available via the `lwip` component this project already depends on for WiFi/HTTP). Sets timezone to China Standard Time (UTC+8, no DST), starts SNTP against a public NTP server, polls `esp_sntp_get_sync_status()` until synced or `timeout_ms` elapses, then converts the resulting `time_t` to `struct tm` and writes it to the RTC via `hal->rtc.setTime()`.

Called once from `main.cpp`, after `WIFI_CONNECT::connect()` succeeds (needs network) and before the Launcher starts. If sync fails/times out (no network, bad NTP response), the RTC just keeps whatever time it already had — no error state needed, this is a best-effort accuracy improvement, not a hard dependency for anything else to function.

## Out of scope

- No manual time-set UI — this is purely automatic (NTP once at boot).
- No screensaver animation/transition effects — content just appears/disappears.
- No periodic weather refresh while the screensaver stays up — fetched once per activation.
- No changes to `IDLE_SCREEN`, any app, or the Launcher's normal carousel rendering/selection logic — purely additive.
