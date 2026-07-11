/**
 * @file ha_client.cpp
 */
#include "ha_client.h"
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"


namespace HA_CLIENT
{
    static const char* TAG = "ha_client";

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

    static esp_http_client_handle_t _make_client(const char* url, const char* token,
                                                  esp_http_client_method_t method,
                                                  ResponseBuffer* resp_buf)
    {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = method;
        config.timeout_ms = 5000;
        config.event_handler = _http_event_handler;
        config.user_data = resp_buf;

        esp_http_client_handle_t client = esp_http_client_init(&config);

        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
        esp_http_client_set_header(client, "Authorization", auth_header);
        esp_http_client_set_header(client, "Content-Type", "application/json");

        return client;
    }

    MediaPlayerState get_state(const char* base_url, const char* token, const char* entity_id)
    {
        MediaPlayerState result;

        char url[256];
        snprintf(url, sizeof(url), "%s/api/states/%s", base_url, entity_id);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_handle_t client = _make_client(url, token, HTTP_METHOD_GET, &resp_buf);
        esp_err_t err = esp_http_client_perform(client);

        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "get_state failed: err=%d status=%d", err, status);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.state = state->valuestring;
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* title = cJSON_GetObjectItem(attributes, "media_title");
            if (cJSON_IsString(title))
            {
                result.title = title->valuestring;
            }

            cJSON* artist = cJSON_GetObjectItem(attributes, "media_artist");
            if (cJSON_IsString(artist))
            {
                result.artist = artist->valuestring;
            }

            cJSON* volume = cJSON_GetObjectItem(attributes, "volume_level");
            if (cJSON_IsNumber(volume))
            {
                result.volume = (float)volume->valuedouble;
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }

    static bool _post_json(const char* base_url, const char* token,
                           const char* path, const char* json_body)
    {
        char url[256];
        snprintf(url, sizeof(url), "%s%s", base_url, path);

        ResponseBuffer resp_buf;
        resp_buf.len = 0;
        resp_buf.data[0] = '\0';

        esp_http_client_handle_t client = _make_client(url, token, HTTP_METHOD_POST, &resp_buf);
        esp_http_client_set_post_field(client, json_body, (int)strlen(json_body));

        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "POST %s failed: err=%d status=%d", path, err, status);
            return false;
        }
        return true;
    }

    bool call_service(const char* base_url, const char* token,
                       const char* domain, const char* service, const char* entity_id)
    {
        char path[128];
        snprintf(path, sizeof(path), "/api/services/%s/%s", domain, service);

        char body[128];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\"}", entity_id);

        return _post_json(base_url, token, path, body);
    }

    bool set_volume(const char* base_url, const char* token,
                     const char* entity_id, float volume)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"volume_level\": %.2f}",
                 entity_id, volume);

        return _post_json(base_url, token, "/api/services/media_player/volume_set", body);
    }
}
