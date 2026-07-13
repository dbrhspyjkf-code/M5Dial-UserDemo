/**
 * @file email_client.cpp
 */
#include "email_client.h"
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace EMAIL_CLIENT
{
    static const char* TAG = "email_client";

    struct ResponseBuffer
    {
        char data[4096];
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

    std::vector<FolderInfo> get_unread(const char* base_url)
    {
        std::vector<FolderInfo> result;

        char url[128];
        snprintf(url, sizeof(url), "%s/api/email/status", base_url);

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
            ESP_LOGE(TAG, "get_unread failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_unread: JSON parse failed");
            return result;
        }

        cJSON* folders = cJSON_GetObjectItem(root, "folders");
        if (cJSON_IsArray(folders))
        {
            int count = cJSON_GetArraySize(folders);
            for (int i = 0; i < count; i++)
            {
                cJSON* item = cJSON_GetArrayItem(folders, i);
                if (item == nullptr) continue;

                FolderInfo info;

                cJSON* name = cJSON_GetObjectItem(item, "name");
                if (cJSON_IsString(name)) info.name = name->valuestring;

                cJSON* unread = cJSON_GetObjectItem(item, "unread");
                if (cJSON_IsNumber(unread)) info.unread = unread->valueint;

                cJSON* subject = cJSON_GetObjectItem(item, "latest_subject");
                if (cJSON_IsString(subject)) info.latest_subject = subject->valuestring;

                cJSON* from = cJSON_GetObjectItem(item, "latest_from");
                if (cJSON_IsString(from)) info.latest_from = from->valuestring;

                result.push_back(info);
            }
        }

        cJSON_Delete(root);
        return result;
    }
}
