/**
 * @file rfid_service.cpp
 */
#include "rfid_service.h"
#include "rfid_service_config.h"
#include "../ha_client/ha_client.h"
#include <rc522.h>
#include <cstdio>
#include <cstring>


namespace RFID_SERVICE
{
    static const char* TAG = "rfid_service";

    static HAL::HAL* s_hal = nullptr;
    static rc522_handle_t s_rc522_handle;
    static uint64_t s_last_scanned_sn = 0;

    static void _turn_off_everything()
    {
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_1);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_2);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_3);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "switch", "turn_off", RFID_SWITCH_4);
        HA_CLIENT::call_service(RFID_HA_BASE_URL, RFID_HA_TOKEN, "fan", "turn_off", RFID_FAN_1);
    }

    static void _event_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
    {
        if (event_id != RC522_EVENT_TAG_SCANNED)
            return;

        rc522_event_data_t* data = (rc522_event_data_t*)event_data;
        rc522_tag_t* tag = (rc522_tag_t*)data->ptr;

        s_last_scanned_sn = tag->serial_number;

        if (s_hal != nullptr)
        {
            s_hal->buzz.tone(4000, 100);
        }

        if (s_last_scanned_sn == RFID_PHONE_CARD_SN)
        {
            _turn_off_everything();
        }
    }

    void init(HAL::HAL* hal)
    {
        s_hal = hal;

        rc522_config_t config;
        memset(&config, 0, sizeof(rc522_config_t));
        config.transport = RC522_TRANSPORT_I2C;
        config.i2c.port = I2C_NUM_0;
        /* Default stack (4KB) overflows once the scan event handler makes
           blocking esp_http_client calls to Home Assistant - give it
           enough room for HTTP header/body allocations. */
        config.task_stack_size = 8192;

        rc522_create(&config, &s_rc522_handle);
        rc522_register_events(s_rc522_handle, RC522_EVENT_ANY, _event_handler, nullptr);
        rc522_start(s_rc522_handle);
    }

    uint64_t last_scanned_sn()
    {
        return s_last_scanned_sn;
    }
}
