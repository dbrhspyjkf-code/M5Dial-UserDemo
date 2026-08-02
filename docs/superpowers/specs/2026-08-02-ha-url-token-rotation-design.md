# HA URL + Token 轮换 — 2026-08-02

## 背景

M5Dial 固件中每个用到 HA 的 App/Service 各自维护一份 gitignored 的
`*_config.h`，里面写死 HA 的 base URL 和 long-lived access token。HA 实例
迁移（IP 变更）+ 老 token 失效，所以**一次性**轮换所有这 4 份。

> ⚠️ 4 个 `*_config.h` 都在 `.gitignore` 里（`Real secrets`，永远不进 commit）。
> 本 spec 只描述"轮换了什么"，**真值只活在本地那 4 个文件里**。

## 改动范围（仅这 4 个文件，每个改 2 行）

| 文件 | 改的宏 |
|---|---|
| `main/apps/app_set_brightness/fishtank_config.h` | `FISHTANK_HA_BASE_URL`, `FISHTANK_HA_TOKEN` |
| `main/apps/app_rtc_test/fan_config.h` | `FAN_HA_BASE_URL`, `FAN_HA_TOKEN` |
| `main/apps/app_sonos/sonos_config.h` | `SONOS_HA_BASE_URL`, `SONOS_HA_TOKEN` |
| `main/apps/utilities/rfid_service/rfid_service_config.h` | `RFID_HA_BASE_URL`, `RFID_HA_TOKEN` |

## 改动内容

- `XXX_HA_BASE_URL`: `http://192.168.1.200:8123` → `http://192.168.1.133:8123`（HA 主机迁移）
- `XXX_HA_TOKEN`: 旧 token 替换为新 token（**本 spec 不写明文；新 token 在 HA 用户 Profile → Security → Long-Lived Access Tokens 页签生成**）

## 不在本次 scope

- **4 份重复没合并** — 之前讨论过"合成一个共享 `ha_config.h`"，本次只做轮换，不重构。后续若 token 再轮换一次觉得烦再独立起 spec。
- **`.example.h` 不动** — 这几个文件里的 `YOUR_HA_HOST` / `YOUR_LONG_LIVED_ACCESS_TOKEN` 是占位符，永远不该有真值。
- **entity_id 不动** — 4 个文件里那 10+ 个 `*_ENTITY_ID` 保持原样。
- **`.cpp` 不动** — `ha_client.*` / 4 个 app 业务代码全不变。
- **WiFi SSID/password 不动** — 还是 `ChinaNet-11G` / `Blackbug225`。

## 用户提醒（来自对话）

> "注意 SONOS 的名字可以不一样了"

**已确认 + 已修（同一天）**：
- `main/apps/app_sonos/sonos_config.h:11`
- `SONOS_ENTITY_ID`: `media_player.ke_ting` → `media_player.ke_ting_ke_ting`
- 5 个 `app_sonos.cpp` 调用点都通过宏引用，零代码改动
- 本 spec 现在覆盖 URL+TOKEN 轮换 + 这个 entity_id 修正

## 验证

- `grep -rn 192.168.1.200 main/` → 应该 0 命中
- `grep -rn 192.168.1.133 main/` → 应该 4 命中（4 个 `XXX_HA_BASE_URL`）
- `git status` 不会显示这 4 个文件（gitignored）—— 这正是预期
- 业务侧：build + flash 后，4 个 app / RFID 卡片扫描 仍能正常打 HA

## 后续 (out of scope，但值得记一下)

- **secret 落 NVS**：token 长期明文落源码不安全（虽然 gitignored，但 IDE、
  backup、误 commit 都有泄漏风险）。后续可以加一个 `nvs_set_blob` 工厂预置流。
- **共享 `ha_config.h`**：4 份重复是历史包袱，重构一次能消掉。
