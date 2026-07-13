/**
 * @file tv_config.example.h
 * @brief Copy this file to tv_config.h (gitignored) and fill in real
 * values. tv_config.h is never committed.
 */
#pragma once

#define TV_WIFI_SSID          "YOUR_WIFI_SSID"
#define TV_WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"
#define TV_HA_BASE_URL        "http://YOUR_HA_HOST:8123"
#define TV_HA_TOKEN           "YOUR_LONG_LIVED_ACCESS_TOKEN"
#define TV_SWITCH_ENTITY_ID   "switch.your_entity"
#define TV_VOLUME_ENTITY_ID   "number.your_entity"

#define TV_NAV_UP_ENTITY_ID     "button.your_tv_press_up"
#define TV_NAV_DOWN_ENTITY_ID   "button.your_tv_press_down"
#define TV_NAV_LEFT_ENTITY_ID   "button.your_tv_press_left"
#define TV_NAV_RIGHT_ENTITY_ID  "button.your_tv_press_right"
#define TV_NAV_OK_ENTITY_ID     "button.your_tv_press_ok"
#define TV_NAV_BACK_ENTITY_ID   "button.your_tv_press_back"
#define TV_NAV_MENU_ENTITY_ID   "button.your_tv_press_menu"
