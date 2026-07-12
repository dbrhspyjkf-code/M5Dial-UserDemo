# RFID Tap-to-Turn-Off + Persistent WiFi — Design

## Context

Today, RFID scanning only happens while `app_rfid_test` is open — `onCreate()` initializes the RC522 reader, `onDestroy()` tears it down. Similarly, WiFi is per-app: `AppSonos`/`Set_Brightness`/`LCD_Test` each `WIFI_CONNECT::connect()` on open and `WIFI_CONNECT::disconnect()` on close, showing a "Connecting..." screen every time.

This adds: (1) a background RFID service that runs continuously from boot, regardless of which app (if any) is open, and (2) persistent boot-time WiFi, so tapping a specific RFID card (the user's phone) at any time — not just while an app is open — turns off 4 switches and a fan via Home Assistant. As a side effect, the 3 existing HA-backed apps stop showing a real "Connecting..." delay, since WiFi is already up.

## Scope decisions

- **RFID becomes a boot-time singleton service**, not owned by any app's lifecycle. `app_rfid_test` (launcher slot 2, "RFID"/"TEST") becomes a thin viewer of the service's "last scanned card" state — it stops creating/destroying the RC522 handle itself.
- **Matching is exact-equality against one hardcoded card** (`355473325619`, the user's phone) — no fuzzy matching, no allowlist of multiple cards for this pass.
- **On match**: buzz once (audible confirmation, since this runs whether or not a screen is being watched), then turn off, in sequence: `switch.xiaomi_cn_945886502_w1_on_p_2_1` (餐厅灯), `switch.xiaomi_cn_945769368_w1_on_p_2_1` (书台灯), `switch.xiaomi_cn_2143401838_w2_on_p_3_1` (筒灯), `switch.xiaomi_cn_2143401838_w2_on_p_2_1` (吸顶灯), `fan.dmaker_cn_740412216_p5c_s_2_fan` (风扇) — the first 4 via `switch.turn_off`, the fan via `fan.turn_off`, both through the existing generic `HA_CLIENT::call_service()`.
- **WiFi connects once at boot** (`main.cpp`, before the launcher starts) and is never disconnected. The 3 existing apps' `onDestroy()` no longer call `WIFI_CONNECT::disconnect()` — leaving it connected is the whole point, and disconnecting on app-close would defeat it. Their `onCreate()`'s existing `WIFI_CONNECT::connect()` call is left as-is (harmless — it returns immediately since `WIFI_CONNECT` already tracks a `s_connected` flag and no-ops if already connected), so no per-app logic needs to change beyond removing that one disconnect line.
- **Safety of doing blocking HTTP calls inside the RC522 event callback**: confirmed safe. The existing `app_rfid_test.cpp`'s event handler already does non-trivial work (buzzer tone + full-screen GUI redraw over SPI) directly inside the `RC522_EVENT_TAG_SCANNED` callback, in production, today — meaning that callback already runs in a normal FreeRTOS task context (esp_event's dispatch task), not an ISR. Blocking HTTP calls in the same callback are therefore safe by the same reasoning; no additional threading/queueing needed.

## Architecture

### New: `main/apps/utilities/rfid_service/`

```
rfid_service.h
rfid_service.cpp
rfid_service_config.example.h   (committed template)
rfid_service_config.h            (gitignored, real values)
```

```cpp
namespace RFID_SERVICE
{
    void init(HAL::HAL* hal);          // create + start the RC522 reader, register the event handler. Called once, at boot.
    uint64_t last_scanned_sn();        // 0 if nothing scanned yet this boot
}
```

Internally: owns the `rc522_handle_t`, registers `RC522_EVENT_TAG_SCANNED`. On every scan: buzz once, store the scanned S/N (for `last_scanned_sn()`), then compare against `RFID_PHONE_CARD_SN` (from `rfid_service_config.h`) — on match, call `HA_CLIENT::call_service()` five times as listed above.

Config (`rfid_service_config.h`, gitignored, reusing the same WiFi/HA credentials already in `sonos_config.h`):
```cpp
#define RFID_HA_BASE_URL   "..."
#define RFID_HA_TOKEN      "..."
#define RFID_PHONE_CARD_SN 355473325619ULL

#define RFID_SWITCH_1 "switch.xiaomi_cn_945886502_w1_on_p_2_1"   // 餐厅灯
#define RFID_SWITCH_2 "switch.xiaomi_cn_945769368_w1_on_p_2_1"   // 书台灯
#define RFID_SWITCH_3 "switch.xiaomi_cn_2143401838_w2_on_p_3_1"  // 筒灯
#define RFID_SWITCH_4 "switch.xiaomi_cn_2143401838_w2_on_p_2_1"  // 吸顶灯
#define RFID_FAN_1    "fan.dmaker_cn_740412216_p5c_s_2_fan"      // 风扇
```

### `main/main.cpp` changes

Before starting the launcher:
```cpp
WIFI_CONNECT::connect(RFID_WIFI_SSID, RFID_WIFI_PASSWORD, 8000);  // persistent, never disconnected
RFID_SERVICE::init(&hal);
```
(WiFi credentials duplicated into `rfid_service_config.h` alongside the HA URL/token/entities, same pattern as every other app's config file — `main.cpp` doesn't reach into `app_sonos`'s config.)

### `app_rfid_test` changes

- `onCreate()`/`onDestroy()` no longer create/destroy an RC522 handle — the handle now belongs to `RFID_SERVICE`, initialized once at boot in `main.cpp`.
- `onRunning()` polls `RFID_SERVICE::last_scanned_sn()` each frame; when it changes from what's currently displayed, re-render the page with the new S/N (same visual as today — "TAG SCANNED" + the number).
- Encoder button still quits, unconditionally, as always.

### 3 existing apps

`AppSonos::onDestroy()`, `Set_Brightness::onDestroy()`, `LCD_Test::onDestroy()`: delete the `WIFI_CONNECT::disconnect();` line from each. Nothing else changes.

## Error handling

If any of the 5 `call_service()` calls fails (WiFi hiccup, HA temporarily down), the service logs it (`ESP_LOGE`) and moves on to the next one — a partial failure (e.g. 4 of 5 lights turn off) is acceptable and not surfaced to the user beyond the log, since there's no dedicated UI for this background action (the buzzer already fired before the calls, confirming the tap was recognized, not that every call succeeded).

## Testing / verification

No unit test harness (same as every feature this session). Verification:
- `idf.py build` per task
- Flash, then with no app open at all (sitting on the launcher menu), tap the phone card against the reader → buzzer sounds, and within a few seconds the 4 switches + fan actually turn off (verify against the real devices, not just HA's state)
- Tap a different/unknown card (or the same phone card doesn't need to be tested twice for "no match" — if the codebase doesn't have another RFID tag on hand, this step can be skipped) — should NOT trigger the light-off action, only the generic buzz+SN update
- Open `app_rfid_test` and tap a card → shows "TAG SCANNED" + the correct S/N, same as before this change
- Open Sonos/fish-tank-light/TV apps → little to no "Connecting..." delay compared to before (WiFi already up)
- Reboot the device → WiFi and RFID service both come up automatically without opening any app
