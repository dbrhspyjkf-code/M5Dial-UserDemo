# Agent Handoff

## Project

`/Users/leenzhou/Projects/M5Dial-UserDemo`

ESP-IDF M5Dial firmware. Current active work is the Reachy app integration.

## Current Reachy App Scope

- The old AC app was replaced by a Reachy app.
- Reachy app pages are ordered: Chat, Video, Audio, Mode, System.
- Reachy suppresses the idle screen while running; exiting Reachy restores normal idle behavior.
- Launcher Reachy app index is `6`.
- Launcher should default to Reachy after boot.

## Important Files

- `main/apps/app_reachy/app_reachy.cpp`
- `main/apps/app_reachy/app_reachy.h`
- `main/apps/app_reachy/gui/gui_reachy.cpp`
- `main/apps/app_reachy/gui/gui_reachy.h`
- `main/apps/utilities/reachy_client/reachy_client.cpp`
- `main/apps/utilities/reachy_client/reachy_client.h`
- `main/apps/launcher/launcher.cpp`
- `main/apps/utilities/smooth_menu/src/simple_menu/simple_menu.cpp`
- `main/apps/utilities/smooth_menu/src/simple_menu/simple_menu.h`
- `test/test_reachy_dial_contract.py`

## Verified Commands

```sh
python3 test/test_reachy_dial_contract.py
```

```sh
. /Users/leenzhou/esp-idf-v5.1.3/export.sh >/tmp/m5dial_idf_export.log && idf.py build
```

```sh
. /Users/leenzhou/esp-idf-v5.1.3/export.sh >/tmp/m5dial_idf_export.log && idf.py -p /dev/cu.usbmodem11401 flash
```

```sh
. /Users/leenzhou/esp-idf-v5.1.3/export.sh >/tmp/m5dial_idf_export.log && timeout 90s idf.py -p /dev/cu.usbmodem11401 monitor
```

## Last Verified State

2026-08-17:

- All pending work (Codex screensaver, timer beep, night idle, NTP fallback,
  TP fix, smooth_menu goToItem, Reachy app) committed as 8 topic commits
  (c136a8b..0713b4b) on top of a496aec.
- sdkconfig NimBLE HID leftovers from the abandoned BLE-scroll experiment
  reverted (CONFIG_BT_NIMBLE_HID_SERVICE / NVS_PERSIST back to unset).
- `python3 test/test_reachy_dial_contract.py` passed with 15 tests.
- `idf.py build` + flash on `/dev/cu.usbmodem11401` OK; boot log shows
  WiFi connect, NTP sync, `launcher: onCreate`, no assert/reboot loop.
- Note: `esptool chip-id --after no-reset` leaves the chip in ROM download
  mode; always use `--after hard-reset` when probing the device.

2026-08-14 (previous agent):

- `python3 test/test_reachy_dial_contract.py` passed with 15 tests.
- `idf.py build` passed.
- Firmware was flashed to `/dev/cu.usbmodem11401`.
- Serial monitor showed pressing the default launcher item opens Reachy: `launcher: selected 6`.
- First Reachy open showed `frame buffer ready capacity=30720`.
- Video page showed repeated `frame ok bytes=...`.
- After exiting and reopening Reachy, the app again showed `frame buffer ready capacity=30720` and Video again showed repeated `frame ok bytes=...`.
- No reboot occurred during the final Video preview verification.

## Reachy UI Behavior

- Chat page shows the latest user/assistant turn.
- Chat page encoder adjusts volume.
- If MIC is disabled and volume is adjusted on Chat, the app enables MIC first.
- Page changes use touch edge swipes only, not encoder rotation.
- Video page hides the backend `Video ON/OFF` flag.
- Audio page includes Volume, MIC enable, and VAD controls.
- Volume and VAD use the edge ring progress indicator.
- Mode page supports XIAOZHI/QWEN and QWEN voice selection.
- Applying mode changes asks for restart confirmation.

## Video Preview Pitfalls

- `/api/conversation/video` returning `video_enabled=false` only means backend video upload is off. It must not block local preview.
- Local preview should fetch `/api/camera/frame` regardless of `video_enabled`.
- `/api/camera/frame` was verified to return JPEG frames from the Reachy/YRobot backend.
- Do not append JPEG chunks with `std::vector::insert()` in the HTTP callback; this can trigger a large contiguous realloc and reset the M5Dial.
- Do not allocate the JPEG frame buffer lazily when entering the Video page; the heap can already be fragmented.
- Current implementation prepares a reusable JPEG buffer in `AppReachy::onCreate()`.
- JPEG buffer size is `30 * 1024`. The previous 32KB version worked on first open but failed after exiting/reopening Reachy because the heap could not provide enough margin.
- If a frame exceeds the buffer, the app should show frame fail/preview waiting, not restart.
- Video rendering uses LovyanGFX `drawJpg(..., -1.0f, -1.0f)` fit scaling instead of custom JPEG size parsing.

## Launcher Default Pitfall

- `Simple_Menu::update()` previously forced the selector back to item `0` on first update, which made the launcher default to TV.
- The fix added `Simple_Menu::goToItem()` and removed the forced first-item reset.
- Launcher calls `goToItem(DEFAULT_SELECTED_APP)` with `DEFAULT_SELECTED_APP = 6`.

## Security

- Do not write Home Assistant tokens, WiFi passwords, or other secrets into this file, tests, logs, commits, or Codex memory.
- If a token must be updated, write only to the intended local config/secret location and avoid echoing it in command output.

## Suggested Next-Agent Startup

1. Read this file first.
2. Run `git status --short` and preserve unrelated user changes.
3. Read the important files listed above before editing.
4. Run `python3 test/test_reachy_dial_contract.py` before and after changes.
5. For firmware changes, build, flash, and verify with serial monitor on the real M5Dial.
