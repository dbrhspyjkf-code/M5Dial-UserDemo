/**
 * @file weather_client.cpp
 */
#include "weather_client.h"
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace WEATHER_CLIENT
{
    static const char* TAG = "weather_client";

    struct ResponseBuffer
    {
        char data[2048];
        int len = 0;
    };

    static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            auto* buf = (ResponseBuffer*)evt->user_data;
            int space = (int)sizeof(buf->data) - buf->len - 1;
            int copy_len = evt->data_len < space ? evt->data_len : space;
            if (copy_len > 0)
            {
                memcpy(buf->data + buf->len, evt->data, copy_len);
                buf->len += copy_len;
                buf->data[buf->len] = '\0';
            }
        }
        return ESP_OK;
    }

    WeatherInfo get_weather(const char* base_url)
    {
        WeatherInfo result;

        char url[128];
        snprintf(url, sizeof(url), "%s/weather", base_url);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000;
        config.event_handler = _http_event_handler;
        config.user_data = &resp_buf;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "get_weather failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_weather: JSON parse failed");
            return result;
        }

        cJSON* temp_c = cJSON_GetObjectItem(root, "temp_c");
        if (cJSON_IsString(temp_c)) result.temp_c = temp_c->valuestring;

        cJSON* condition = cJSON_GetObjectItem(root, "condition");
        if (cJSON_IsString(condition)) result.condition = condition->valuestring;

        cJSON* ok = cJSON_GetObjectItem(root, "ok");
        result.ok = cJSON_IsTrue(ok) && !result.temp_c.empty();

        cJSON_Delete(root);
        return result;
    }
}
