/**
 * @file rfid_service_config.example.h
 * @brief Copy this file to rfid_service_config.h (gitignored) and fill
 * in real values. rfid_service_config.h is never committed.
 */
#pragma once

#define RFID_WIFI_SSID       "YOUR_WIFI_SSID"
#define RFID_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define RFID_HA_BASE_URL     "http://YOUR_HA_HOST:8123"
#define RFID_HA_TOKEN        "YOUR_LONG_LIVED_ACCESS_TOKEN"

/* The card that triggers the turn-off action (decimal serial number,
   as shown by app_rfid_test when you scan it) */
#define RFID_PHONE_CARD_SN   0ULL

#define RFID_SWITCH_1   "switch.your_entity_1"
#define RFID_SWITCH_2   "switch.your_entity_2"
#define RFID_SWITCH_3   "switch.your_entity_3"
#define RFID_SWITCH_4   "switch.your_entity_4"
#define RFID_FAN_1      "fan.your_entity"
