# TV Nav Remote (TV/CTRL extension) Design

## Goal

Extend the existing TV control app (`LCD_Test`/`GUI_LCD_Test`, launcher tag "TV"/"CTRL") with a second page of D-pad-style navigation controls (up/down/left/right/OK/back/menu), matching the reference remote-control image the user provided. Also remove the white bubble background, switching to the flat themed-background style already used by the Timer/Stock apps.

## HA entities

Confirmed via a live `GET /api/states` query against the HA server — all are `button` domain, momentary/stateless "press" actions, no state to read back:

- `button.xiaomi_cn_629973618_esprh1_press_up_a_7_8` — UP
- `button.xiaomi_cn_629973618_esprh1_press_down_a_7_9` — DOWN
- `button.xiaomi_cn_629973618_esprh1_press_left_a_7_6` — LEFT
- `button.xiaomi_cn_629973618_esprh1_press_right_a_7_7` — RIGHT
- `button.xiaomi_cn_629973618_esprh1_press_ok_a_7_10` — OK/ENTER
- `button.xiaomi_cn_629973618_esprh1_press_back_a_7_5` — BACK
- `button.xiaomi_cn_629973618_esprh1_press_menu_a_7_3` — MENU

Service: `button.press` with just `{"entity_id": entity_id}` — this is exactly `HA_CLIENT::call_service(base_url, token, "button", "press", entity_id)`, already implemented and used elsewhere (e.g. the fan/light turn_off calls). **No new HA_CLIENT code needed.**

## Pages

Two pages, toggled by a touch button (same "MODE toggles between two views" convention as the fish-tank-light app's BRIGHTNESS/EFFECT pages):

**VOLUME page** (existing, restyled flat — no white bubble):
- "VOLUME" label, target volume % (large), "TV" label — same content as today, just redrawn directly on the themed background instead of inside a white rounded-rect bubble (matching the Timer/Stock apps' style).
- Bottom row: `MODE` (switches to NAV page) and `POWER` buttons, same position convention as the FAN/AC apps (x=78 and x=162, y=206).
- Encoder still adjusts volume here (unchanged behavior, debounced).

**NAV page** (new):
- Small "NAV" label near the top.
- A D-pad cross layout centered around (120, 110): UP above, DOWN below, LEFT and RIGHT to the sides, OK in the center. Each is a small filled circle/rounded-rect with its label ("^", "v", "<", ">", "OK").
- Bottom row of 3 buttons: `BACK`, `MENU`, and `VOL` (switches back to the VOLUME page).
- Every button press calls `HA_CLIENT::call_service(..., "button", "press", <entity>)` directly — no state refresh needed afterward (these are momentary actions with no meaningful state to display).
- Encoder does nothing on this page (no continuous value to adjust) — only the encoder button (quit) and touch matter here.

## Touch zone layout

Screen is 240×240 (round). Established fixed reference points from other apps: top icon at y=24, quit "<" at y=215.

- VOLUME page bottom row: y=206, same MODE(x=78)/POWER(x=162) touch zones as FAN/AC (x 30-118 / 122-210).
- NAV page D-pad: UP/DOWN/LEFT/RIGHT/OK arranged in a compact cross (roughly 30px pitch) between y=80 and y=140, x range 88-152.
- NAV page bottom row: y=193, three buttons (BACK/MENU/VOL) roughly at x=55/120/185.

Exact pixel touch-zone boundaries are worked out during implementation (same iterative on-device photo-feedback process used for every prior app's layout).

## Out of scope

- No visual feedback/highlight beyond the existing quick-flash-on-press convention (buttons don't need to show a "state" since they're momentary).
- No changes to `HA_CLIENT`, config files, or the launcher tag/icon — this is purely an in-app addition to the existing TV control app.
- Class names (`LCD_Test`/`GUI_LCD_Test`) and launcher wiring stay unchanged.
