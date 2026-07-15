/**
 * @file wifi_connect_wrap.cpp
 */
#include "wifi_connect_wrap.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"


namespace WIFI_CONNECT
{
    static const char* TAG = "wifi_connect_wrap";

    static EventGroupHandle_t s_event_group = nullptr;
    static esp_netif_t* s_sta_netif = nullptr;
    static bool s_connected = false;
    static bool s_initialized = false;

    #define WIFI_CONNECTED_BIT BIT0
    #define WIFI_FAIL_BIT       BIT1

    static void event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            /* By default the STA does NOT auto-reconnect after this event -
               without explicitly retrying here, one transient drop (AP
               reboot, roam, interference) leaves the radio disconnected
               forever, while s_connected below still latches true from the
               original connect() call. Every app's HA_CLIENT call then
               fails with "unreachable" until the device is power-cycled. */
            s_connected = false;
            xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(s_event_group, WIFI_FAIL_BIT);
            esp_wifi_connect();
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            s_connected = true;
            xEventGroupClearBits(s_event_group, WIFI_FAIL_BIT);
            xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
        }
    }

    bool connect(const char* ssid, const char* password, uint32_t timeout_ms)
    {
        if (s_connected)
        {
            return true;
        }

        if (s_initialized)
        {
            /* WiFi stack is already up (e.g. after a prior connect() call)
               and a reconnect is already in flight from event_handler above
               - just wait for it instead of re-running init, which would
               hit ESP_ERROR_CHECK failures on the already-initialized
               netif/event loop/wifi driver. */
            xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
            esp_wifi_connect();

            EventBits_t bits = xEventGroupWaitBits(
                s_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE, pdFALSE,
                pdMS_TO_TICKS(timeout_ms));

            s_connected = (bits & WIFI_CONNECTED_BIT) != 0;
            if (!s_connected)
            {
                ESP_LOGE(TAG, "reconnect failed or timed out");
            }
            return s_connected;
        }

        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        s_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_sta_netif = esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config));
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        EventBits_t bits = xEventGroupWaitBits(
            s_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(timeout_ms));

        s_connected = (bits & WIFI_CONNECTED_BIT) != 0;
        s_initialized = true;

        if (!s_connected)
        {
            ESP_LOGE(TAG, "connect failed or timed out");
        }
        else
        {
            /* ESP-IDF defaults to WIFI_PS_MIN_MODEM (modem sleep) - the
               radio dozes between the AP's beacon intervals to save
               power, and a fresh outbound packet (e.g. the very first
               SYN when an app opens a new HTTP connection) can end up
               waiting for the radio to wake before it actually goes out.
               Confirmed live: even a one-shot client with zero
               connection reuse (weather_client) was failing with
               "esp-tls: select() timeout" / "Failed to open a new
               connection" on fresh connects - this device is USB-powered
               anyway, so there's no reason to trade connection latency
               for power savings it doesn't need. */
            esp_wifi_set_ps(WIFI_PS_NONE);
        }

        return s_connected;
    }

    void disconnect()
    {
        if (!s_initialized)
        {
            return;
        }

        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(s_sta_netif);
        esp_netif_deinit();
        esp_event_loop_delete_default();
        nvs_flash_deinit();

        vEventGroupDelete(s_event_group);
        s_event_group = nullptr;
        s_sta_netif = nullptr;
        s_connected = false;
        s_initialized = false;
    }
}
