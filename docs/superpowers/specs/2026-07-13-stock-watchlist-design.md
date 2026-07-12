# Stock Watchlist (MORE → STOCK) Design

## Goal

Repurpose `app_more_menu` (currently a placeholder demo menu with items like "LVGL Widgets"/"Power Off" that don't do anything meaningful) into a read-only stock watchlist viewer, pulling live quotes from the user's existing `hermes-mcp-xiaozhi` service. Same repurposing pattern as the earlier LCD-TEST→TV, BRIGHTNESS-SET→fish-tank-light, RTC-TEST→FAN, and TEMP-DEMO→AC features in this project — except this is a **read-only display**, not an HA control app, and the data source is a different service (not Home Assistant).

## Data source

`GET http://192.168.1.200:8766/api/stocks/portfolio` — confirmed reachable and working with no authentication required (unlike the HA REST calls, which need a Bearer token). Response shape (confirmed via live `curl`):

```json
{"count": 11, "items": [
  {"code": "600456", "name": "宝馨股份", "price": 27.8, "chg": 5.34, "pchg": 1.41,
   "turnover": 0.0, "liangbi": 0.0,
   "one_sentence": "...", "analysis_summary": "...", "analysis_date": "2026-07-10"},
  ...
]}
```

Only `code`, `name`, `price`, and `chg` (percentage change) are used — `pchg`/`turnover`/`liangbi`/`one_sentence`/`analysis_summary`/`analysis_date` are ignored per this feature's scope (display-only, no analysis text).

## STOCK_CLIENT utility

New `main/apps/utilities/stock_client/stock_client.h`/`.cpp`. Unlike `HA_CLIENT`, this does **not** use a persistent connection: the actual response body is ~12KB (confirmed via live `curl` — the server includes `analysis_summary`/`one_sentence` text for every stock even though this app doesn't display it), far larger than `HA_CLIENT`'s fixed 2048-byte buffer, and this endpoint is only ever called once per app open (not continuously polled like the 4 HA apps), so the connection-leak problem `HA_CLIENT`'s persistent-connection fix addressed doesn't apply here — a fresh create-per-call client is fine at this call frequency.

- `StockItem{code, name, price, chg}`
- `std::vector<StockItem> get_portfolio(base_url)` — creates a one-off `esp_http_client`, GETs the endpoint into a `malloc`'d 16KB response buffer (freed right after parsing — too large to safely put on a task's stack, and not worth reserving statically for an app that's rarely open), parses the JSON array, returns the list (empty list on failure). No `init()` needed — nothing to set up ahead of time.

## App behavior

- **Fetch**: once per app open, in `onCreate()` (no periodic auto-refresh — matches the user's explicit choice).
- **Render immediately** with a "Loading..." status screen while fetching (this is a one-shot fetch, not a per-tick poll, so unlike the 4 HA control apps there's no "render with stale defaults" option on first-ever open — nothing has been fetched yet this session).
- **Encoder rotate**: moves to the next/previous stock in the list, **wrapping around** (rotating past the last stock goes back to the first, and vice versa) — a natural fit for browsing a fixed list, unlike the continuous-range controls (volume/brightness/temperature) in the other apps.
- **Display**: stock name (falls back to code if name is empty) large in the middle, price below it, percentage change below that — colored **red for positive change, green for negative change** (A-share market convention — the opposite of the US convention, and the correct one for this user's data). A small "X / N" position indicator (e.g. "3 / 11") so the user knows where they are in the list.
- **Error handling**: if the fetch fails or returns zero items, show an error screen with "TAP TO RETRY" (same convention as the 4 HA apps' `ERROR` state), tapping retries the fetch.
- **Quit**: encoder button press-and-release, same project-wide convention as every other app.
- No touch-driven actions beyond the retry tap — this app is read-only, so there's no POWER/MODE button row like the HA control apps.

## Config

New `main/apps/app_more_menu/stock_config.h` (gitignored) + `stock_config.example.h` (committed template) — just `STOCK_API_BASE_URL = "http://192.168.1.200:8766"`. No WiFi credentials needed here (WiFi is already connected persistently from boot via `main.cpp`, shared across the whole device), no auth token needed (this endpoint doesn't require one).

## Launcher

- Tag renamed `"MORE", ""` → `"STOCK", "WATCH"`.
- Icon replaced with a new stock/chart icon (`icon_stock.h`, generated from the user-supplied `icons8-股市-100.png`), colored to match this launcher slot's existing theme color `0x5D7BA2` (same "match the icon accent to the slot's theme color" convention used for the FAN icon) — same solid-disc-fill + white-glyph style as every other launcher icon.

## Out of scope

- No periodic/background refresh — matches the user's explicit choice (fetch once per open).
- No display of `analysis_summary`/`one_sentence`/`analysis_date` — display-only for name/price/change per the user's explicit field choice.
- No renaming of the `MoreMenu` C++ class name — keeps the launcher wiring (`case 7`, no `getGui()` override currently) — this feature **adds** a proper `GUI_Base`-derived GUI class for `MoreMenu` (it doesn't have one today, since the old placeholder menu drew directly to the raw canvas via a different `SMOOTH_MENU` widget system), matching the `getGui()` convention every other app already follows. This is the one structural change beyond a pure "swap the body" repurpose, needed because the old demo never had a real `GUI_Base` subclass to begin with.
