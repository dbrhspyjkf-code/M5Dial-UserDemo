/**
 * @file codex_client.cpp
 */
#include "codex_client.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace CODEX_CLIENT
{
    static const char* TAG = "codex_client";

    struct ResponseBuffer
    {
        char data[512];
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

    Usage fetch(const char* server_url)
    {
        Usage result;

        char url[128];
        snprintf(url, sizeof(url), "%s/api/codex/usage", server_url);

        ResponseBuffer resp_buf;

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        /* The server reads local JSONL files and responds in <10ms on
           the LAN — 3s is generous and avoids blocking the UI loop. */
        config.timeout_ms = 3000;
        config.event_handler = _http_event_handler;
        config.user_data = &resp_buf;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "fetch failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "fetch: JSON parse failed");
            return result;
        }

        cJSON* ok = cJSON_GetObjectItem(root, "ok");
        if (!cJSON_IsTrue(ok))
        {
            cJSON_Delete(root);
            return result;
        }

        cJSON* used = cJSON_GetObjectItem(root, "used");
        if (cJSON_IsNumber(used)) result.used = (float)used->valuedouble;

        cJSON* remaining = cJSON_GetObjectItem(root, "remaining");
        if (cJSON_IsNumber(remaining)) result.remaining = (float)remaining->valuedouble;

        cJSON* window = cJSON_GetObjectItem(root, "window");
        if (cJSON_IsString(window)) result.window = window->valuestring;

        cJSON* reset = cJSON_GetObjectItem(root, "reset");
        if (cJSON_IsString(reset)) result.reset = reset->valuestring;

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }
}
