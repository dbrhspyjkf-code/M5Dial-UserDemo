/**
 * @file deepseek_client.cpp
 */
#include "deepseek_client.h"
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace DEEPSEEK_CLIENT
{
    static const char* TAG = "deepseek_client";

    struct ResponseBuffer
    {
        char data[1024];
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

    BalanceInfo get_balance(const char* base_url)
    {
        BalanceInfo result;

        char url[128];
        snprintf(url, sizeof(url), "%s/api/deepseek/balance", base_url);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 8000;
        config.event_handler = _http_event_handler;
        config.user_data = &resp_buf;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "get_balance failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_balance: JSON parse failed");
            return result;
        }

        cJSON* ok = cJSON_GetObjectItem(root, "ok");
        if (!cJSON_IsTrue(ok))
        {
            cJSON_Delete(root);
            return result;
        }

        cJSON* currency = cJSON_GetObjectItem(root, "currency");
        if (cJSON_IsString(currency)) result.currency = currency->valuestring;

        cJSON* total = cJSON_GetObjectItem(root, "total_balance");
        if (cJSON_IsString(total)) result.total_balance = total->valuestring;

        cJSON* granted = cJSON_GetObjectItem(root, "granted_balance");
        if (cJSON_IsString(granted)) result.granted_balance = granted->valuestring;

        cJSON* topped_up = cJSON_GetObjectItem(root, "topped_up_balance");
        if (cJSON_IsString(topped_up)) result.topped_up_balance = topped_up->valuestring;

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }
}
