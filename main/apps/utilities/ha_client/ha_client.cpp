/**
 * @file ha_client.cpp
 */
#include "ha_client.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


namespace HA_CLIENT
{
    static const char* TAG = "ha_client";
    static esp_http_client_handle_t s_client = nullptr;
    static SemaphoreHandle_t s_mutex = nullptr;

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

    void init()
    {
        if (s_client != nullptr)
            return;

        s_mutex = xSemaphoreCreateMutex();

        esp_http_client_config_t config = {};
        config.url = "http://localhost/"; /* overwritten before every perform() */
        /* Home Assistant is on the same LAN - a healthy request completes
           in well under a second. Kept short so a stuck/slow request
           can't block an app's quit button (checked after any in-flight
           call returns) for longer than this. */
        config.timeout_ms = 2500;
        config.event_handler = _http_event_handler;

        s_client = esp_http_client_init(&config);
    }

    /**
     * @brief Perform one request on the shared, persistent client. Mutex
     * serializes every caller (RFID_SERVICE's background task and
     * whichever app is open both call into this).
     */
    static bool _perform(const char* base_url, const char* token, const char* path,
                         esp_http_client_method_t method, const char* body,
                         ResponseBuffer* resp_buf)
    {
        xSemaphoreTake(s_mutex, portMAX_DELAY);

        char url[256];
        snprintf(url, sizeof(url), "%s%s", base_url, path);

        resp_buf->len = 0;
        resp_buf->data[0] = '\0';

        esp_http_client_set_url(s_client, url);
        esp_http_client_set_method(s_client, method);
        esp_http_client_set_user_data(s_client, resp_buf);

        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
        esp_http_client_set_header(s_client, "Authorization", auth_header);
        esp_http_client_set_header(s_client, "Content-Type", "application/json");

        if (body != nullptr)
        {
            esp_http_client_set_post_field(s_client, body, (int)strlen(body));
        }
        else
        {
            esp_http_client_set_post_field(s_client, nullptr, 0);
        }

        esp_err_t err = esp_http_client_perform(s_client);
        int status = esp_http_client_get_status_code(s_client);

        /* Close the underlying transport after every call instead of
           leaving the keep-alive socket open across calls. HA (aiohttp)
           closes idle keep-alive connections after its own timeout, but
           this client handle has no way to know that happened - reusing
           the now-dead socket on the next call was failing with
           ESP_ERR_HTTP_EAGAIN (0x7007) once an app had been closed for a
           while. esp_http_client_close() only tears down the transport
           (cheap), not the whole handle/header list like a full
           cleanup+init would (that was the original per-call leak this
           persistent client was introduced to avoid) - the next call
           just reconnects fresh. */
        esp_http_client_close(s_client);

        xSemaphoreGive(s_mutex);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "%s failed: err=%d status=%d", path, err, status);
            return false;
        }
        return true;
    }

    MediaPlayerState get_state(const char* base_url, const char* token, const char* entity_id)
    {
        MediaPlayerState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
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
        ResponseBuffer resp_buf;
        return _perform(base_url, token, path, HTTP_METHOD_POST, json_body, &resp_buf);
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


    LightState get_light_state(const char* base_url, const char* token, const char* entity_id)
    {
        LightState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_light_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.is_on = (strcmp(state->valuestring, "on") == 0);
        }

        if (result.is_on)
        {
            cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
            if (attributes != nullptr)
            {
                cJSON* brightness = cJSON_GetObjectItem(attributes, "brightness");
                if (cJSON_IsNumber(brightness))
                {
                    /* HA's light.brightness is 0-255; convert to 0-100 */
                    result.brightness_pct = (brightness->valueint * 100 + 127) / 255;
                }

                cJSON* effect = cJSON_GetObjectItem(attributes, "effect");
                if (cJSON_IsString(effect))
                {
                    result.effect = effect->valuestring;
                }

                cJSON* effect_list = cJSON_GetObjectItem(attributes, "effect_list");
                if (cJSON_IsArray(effect_list))
                {
                    int count = cJSON_GetArraySize(effect_list);
                    for (int i = 0; i < count; i++)
                    {
                        cJSON* item = cJSON_GetArrayItem(effect_list, i);
                        if (cJSON_IsString(item))
                        {
                            result.effect_list.push_back(item->valuestring);
                        }
                    }
                }
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_light_brightness(const char* base_url, const char* token,
                               const char* entity_id, int brightness_pct)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"brightness_pct\": %d}",
                 entity_id, brightness_pct);

        return _post_json(base_url, token, "/api/services/light/turn_on", body);
    }


    bool set_light_power(const char* base_url, const char* token,
                          const char* entity_id, bool on)
    {
        return call_service(base_url, token, "light", on ? "turn_on" : "turn_off", entity_id);
    }


    bool set_light_effect(const char* base_url, const char* token,
                           const char* entity_id, const char* effect_name)
    {
        char body[192];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"effect\": \"%s\"}",
                 entity_id, effect_name);

        return _post_json(base_url, token, "/api/services/light/turn_on", body);
    }


    SwitchState get_switch_state(const char* base_url, const char* token, const char* entity_id)
    {
        SwitchState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_switch_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.is_on = (strcmp(state->valuestring, "on") == 0);
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    NumberState get_number_state(const char* base_url, const char* token, const char* entity_id)
    {
        NumberState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_number_state: JSON parse failed");
            return result;
        }

        /* For the number domain, top-level "state" IS the value, as a
           numeric string (e.g. "45.0"), not "on"/"off". */
        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.value = strtof(state->valuestring, nullptr);
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* min_attr = cJSON_GetObjectItem(attributes, "min");
            if (cJSON_IsNumber(min_attr))
            {
                result.min = (float)min_attr->valuedouble;
            }

            cJSON* max_attr = cJSON_GetObjectItem(attributes, "max");
            if (cJSON_IsNumber(max_attr))
            {
                result.max = (float)max_attr->valuedouble;
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_number_value(const char* base_url, const char* token,
                           const char* entity_id, float value)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"value\": %.2f}",
                 entity_id, value);

        return _post_json(base_url, token, "/api/services/number/set_value", body);
    }


    FanState get_fan_state(const char* base_url, const char* token, const char* entity_id)
    {
        FanState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_fan_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.is_on = (strcmp(state->valuestring, "on") == 0);
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* percentage = cJSON_GetObjectItem(attributes, "percentage");
            if (cJSON_IsNumber(percentage))
            {
                result.percentage = percentage->valueint;
            }

            cJSON* oscillating = cJSON_GetObjectItem(attributes, "oscillating");
            if (cJSON_IsBool(oscillating))
            {
                result.oscillating = cJSON_IsTrue(oscillating);
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_fan_power(const char* base_url, const char* token,
                        const char* entity_id, bool on)
    {
        return call_service(base_url, token, "fan", on ? "turn_on" : "turn_off", entity_id);
    }


    bool set_fan_percentage(const char* base_url, const char* token,
                             const char* entity_id, int percentage)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"percentage\": %d}",
                 entity_id, percentage);

        return _post_json(base_url, token, "/api/services/fan/set_percentage", body);
    }


    bool set_fan_oscillating(const char* base_url, const char* token,
                              const char* entity_id, bool oscillating)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"oscillating\": %s}",
                 entity_id, oscillating ? "true" : "false");

        return _post_json(base_url, token, "/api/services/fan/oscillate", body);
    }


    ClimateState get_climate_state(const char* base_url, const char* token, const char* entity_id)
    {
        ClimateState result;

        char path[192];
        snprintf(path, sizeof(path), "/api/states/%s", entity_id);

        ResponseBuffer resp_buf;
        if (!_perform(base_url, token, path, HTTP_METHOD_GET, nullptr, &resp_buf))
        {
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_climate_state: JSON parse failed");
            return result;
        }

        cJSON* state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state))
        {
            result.hvac_mode = state->valuestring;
        }

        cJSON* attributes = cJSON_GetObjectItem(root, "attributes");
        if (attributes != nullptr)
        {
            cJSON* target_temp = cJSON_GetObjectItem(attributes, "temperature");
            if (cJSON_IsNumber(target_temp))
            {
                result.target_temp = (float)target_temp->valuedouble;
            }

            cJSON* current_temp = cJSON_GetObjectItem(attributes, "current_temperature");
            if (cJSON_IsNumber(current_temp))
            {
                result.current_temp = (float)current_temp->valuedouble;
            }

            cJSON* min_temp = cJSON_GetObjectItem(attributes, "min_temp");
            if (cJSON_IsNumber(min_temp))
            {
                result.min_temp = (float)min_temp->valuedouble;
            }

            cJSON* max_temp = cJSON_GetObjectItem(attributes, "max_temp");
            if (cJSON_IsNumber(max_temp))
            {
                result.max_temp = (float)max_temp->valuedouble;
            }

            cJSON* hvac_modes = cJSON_GetObjectItem(attributes, "hvac_modes");
            if (cJSON_IsArray(hvac_modes))
            {
                int count = cJSON_GetArraySize(hvac_modes);
                for (int i = 0; i < count; i++)
                {
                    cJSON* item = cJSON_GetArrayItem(hvac_modes, i);
                    if (cJSON_IsString(item))
                    {
                        result.hvac_modes.push_back(item->valuestring);
                    }
                }
            }
        }

        cJSON_Delete(root);
        result.ok = true;
        return result;
    }


    bool set_climate_temperature(const char* base_url, const char* token,
                                  const char* entity_id, float temperature)
    {
        char body[160];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"temperature\": %.1f}",
                 entity_id, temperature);

        return _post_json(base_url, token, "/api/services/climate/set_temperature", body);
    }


    bool set_climate_hvac_mode(const char* base_url, const char* token,
                                const char* entity_id, const char* hvac_mode)
    {
        char body[192];
        snprintf(body, sizeof(body), "{\"entity_id\": \"%s\", \"hvac_mode\": \"%s\"}",
                 entity_id, hvac_mode);

        return _post_json(base_url, token, "/api/services/climate/set_hvac_mode", body);
    }
}
