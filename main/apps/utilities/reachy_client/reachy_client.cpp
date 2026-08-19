/**
 * @file reachy_client.cpp
 */
#include "reachy_client.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace REACHY_CLIENT
{
    static const char* TAG = "reachy_client";

    struct ResponseParser
    {
        std::string pending;
        ChatTurn turn;
    };

    struct TextResponse
    {
        std::string body;
        size_t max_len = 4096;
    };

    static esp_err_t _text_event_handler(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            auto* resp = (TextResponse*)evt->user_data;
            size_t room = resp->max_len > resp->body.size() ? resp->max_len - resp->body.size() : 0;
            if (room == 0) return ESP_OK;
            size_t copy_len = evt->data_len < (int)room ? evt->data_len : room;
            resp->body.append((const char*)evt->data, copy_len);
        }
        return ESP_OK;
    }

    static OperationResult _request_json(const char* base_url, const char* path,
                                         esp_http_client_method_t method,
                                         const char* body,
                                         std::string* response = nullptr)
    {
        char url[192];
        snprintf(url, sizeof(url), "%s%s", base_url, path);

        TextResponse text;
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = method;
        config.timeout_ms = 3000;
        config.event_handler = _text_event_handler;
        config.user_data = &text;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (method == HTTP_METHOD_PUT || method == HTTP_METHOD_POST)
            esp_http_client_set_header(client, "Content-Type", "application/json");
        if (body != nullptr)
            esp_http_client_set_post_field(client, body, strlen(body));

        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (response != nullptr)
            *response = std::move(text.body);
        return {err == ESP_OK && status >= 200 && status < 300, status};
    }

    static int _json_int(cJSON* obj, const char* name, int fallback)
    {
        cJSON* item = cJSON_GetObjectItem(obj, name);
        return cJSON_IsNumber(item) ? item->valueint : fallback;
    }

    static float _json_float(cJSON* obj, const char* name, float fallback)
    {
        cJSON* item = cJSON_GetObjectItem(obj, name);
        return cJSON_IsNumber(item) ? (float)item->valuedouble : fallback;
    }

    static bool _json_bool(cJSON* obj, const char* name, bool fallback)
    {
        cJSON* item = cJSON_GetObjectItem(obj, name);
        return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
    }

    static std::string _json_string(cJSON* obj, const char* name, const std::string& fallback)
    {
        cJSON* item = cJSON_GetObjectItem(obj, name);
        return cJSON_IsString(item) && item->valuestring ? item->valuestring : fallback;
    }

    static void _trim(std::string& text)
    {
        auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            text.clear();
            return;
        }
        text.erase(0, first);
        auto last = text.find_last_not_of(" \t\r\n");
        if (last != std::string::npos)
            text.erase(last + 1);
    }

    static std::string _json_unescape(const std::string& raw)
    {
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); i++)
        {
            if (raw[i] != '\\' || i + 1 >= raw.size())
            {
                out += raw[i];
                continue;
            }

            char escaped = raw[++i];
            switch (escaped)
            {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                default:
                    out += '\\';
                    out += escaped;
                    break;
            }
        }
        return out;
    }

    static bool _extract_marker(const std::string& line, bool& is_user, std::string& text)
    {
        struct Marker
        {
            const char* token;
            bool user;
        };
        static const Marker markers[] = {
            {"xz stt:", true},
            {"xz tts text:", false},
            {"qwen stt:", true},
            {"qwen response:", false},
        };

        for (auto& marker : markers)
        {
            auto pos = line.find(marker.token);
            if (pos == std::string::npos)
                continue;

            text = line.substr(pos + strlen(marker.token));
            _trim(text);
            if (text.empty())
                return false;
            if (!marker.user && text.rfind("% tool", 0) == 0)
                return false;
            if (!marker.user && text.rfind("% ", 0) == 0)
                return false;

            is_user = marker.user;
            return true;
        }
        return false;
    }

    static void _apply_message(ResponseParser& parser, const std::string& message)
    {
        bool is_user = false;
        std::string text;
        if (!_extract_marker(message, is_user, text))
            return;

        if (is_user)
        {
            parser.turn.user = std::move(text);
            parser.turn.assistant.clear();
        }
        else if (!parser.turn.user.empty())
        {
            parser.turn.assistant = std::move(text);
        }
        else
        {
            parser.turn.assistant = std::move(text);
        }
    }

    static void _consume_messages(ResponseParser& parser)
    {
        static const char* KEY = "\"message\":\"";
        static const size_t KEY_LEN = 11;

        while (true)
        {
            size_t pos = parser.pending.find(KEY);
            if (pos == std::string::npos)
            {
                if (parser.pending.size() > 64)
                    parser.pending.erase(0, parser.pending.size() - 64);
                return;
            }

            size_t start = pos + KEY_LEN;
            bool escaped = false;
            size_t end = start;
            for (; end < parser.pending.size(); end++)
            {
                char c = parser.pending[end];
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (c == '"')
                    break;
            }

            if (end >= parser.pending.size())
            {
                if (pos > 0)
                    parser.pending.erase(0, pos);
                return;
            }

            _apply_message(parser, _json_unescape(parser.pending.substr(start, end - start)));
            parser.pending.erase(0, end + 1);
        }
    }

    static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            auto* parser = (ResponseParser*)evt->user_data;
            parser->pending.append((const char*)evt->data, evt->data_len);
            _consume_messages(*parser);
        }
        return ESP_OK;
    }

    ChatTurn fetch_recent_turn(const char* base_url, int limit)
    {
        char url[192];
        snprintf(url, sizeof(url), "%s/api/logs?filter=chat&limit=%d", base_url, limit);

        ResponseParser parser;

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000;
        config.event_handler = _http_event_handler;
        config.user_data = &parser;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200)
        {
            ESP_LOGE(TAG, "fetch_recent_turn failed: err=%d status=%d", err, status);
            return parser.turn;
        }

        _consume_messages(parser);
        parser.turn.ok = !parser.turn.user.empty() || !parser.turn.assistant.empty();
        return parser.turn;
    }

    AudioState fetch_audio_state(const char* base_url)
    {
        AudioState audio;
        std::string body;

        auto volume = _request_json(base_url, "/api/volume", HTTP_METHOD_GET, nullptr, &body);
        if (volume.ok)
        {
            cJSON* root = cJSON_Parse(body.c_str());
            cJSON* v = root ? cJSON_GetObjectItem(root, "volume") : nullptr;
            if (cJSON_IsObject(v))
                audio.volume_percent = _json_int(v, "percent", audio.volume_percent);
            if (root) cJSON_Delete(root);
        }

        auto input = _request_json(base_url, "/api/audio/input", HTTP_METHOD_GET, nullptr, &body);
        if (input.ok)
        {
            cJSON* root = cJSON_Parse(body.c_str());
            cJSON* a = root ? cJSON_GetObjectItem(root, "audio_input") : nullptr;
            if (cJSON_IsObject(a))
                audio.mic_enabled = _json_bool(a, "enabled", audio.mic_enabled);
            if (root) cJSON_Delete(root);
        }

        auto vad = _request_json(base_url, "/api/audio/vad", HTTP_METHOD_GET, nullptr, &body);
        if (vad.ok)
        {
            cJSON* root = cJSON_Parse(body.c_str());
            cJSON* v = root ? cJSON_GetObjectItem(root, "vad") : nullptr;
            if (cJSON_IsObject(v))
                audio.vad_rms_min = _json_float(v, "rms_min", audio.vad_rms_min);
            if (root) cJSON_Delete(root);
        }

        audio.ok = volume.ok || input.ok || vad.ok;
        return audio;
    }

    ModeState fetch_mode_state(const char* base_url)
    {
        ModeState mode;
        std::string body;

        auto backend = _request_json(base_url, "/api/conversation/backend", HTTP_METHOD_GET, nullptr, &body);
        if (backend.ok)
        {
            cJSON* root = cJSON_Parse(body.c_str());
            if (root)
            {
                mode.configured_backend = _json_string(root, "configured_backend", mode.configured_backend);
                mode.running_backend = _json_string(root, "running_backend", mode.running_backend);
                cJSON_Delete(root);
            }
        }

        auto voice = _request_json(base_url, "/api/conversation/voice", HTTP_METHOD_GET, nullptr, &body);
        if (voice.ok)
        {
            cJSON* root = cJSON_Parse(body.c_str());
            if (root)
            {
                mode.configured_voice = _json_string(root, "configured_voice", mode.configured_voice);
                cJSON* voices = cJSON_GetObjectItem(root, "available_voices");
                if (cJSON_IsArray(voices))
                {
                    cJSON* item = nullptr;
                    cJSON_ArrayForEach(item, voices)
                    {
                        if (cJSON_IsString(item) && item->valuestring)
                            mode.available_voices.emplace_back(item->valuestring);
                    }
                }
                cJSON_Delete(root);
            }
        }

        mode.ok = backend.ok || voice.ok;
        if (mode.configured_voice.empty() && !mode.available_voices.empty())
            mode.configured_voice = mode.available_voices[0];
        return mode;
    }

    OperationResult set_volume(const char* base_url, int percent)
    {
        char body[32];
        snprintf(body, sizeof(body), "{\"percent\":%d}", percent);
        return _request_json(base_url, "/api/volume", HTTP_METHOD_PUT, body);
    }

    OperationResult set_mic_enabled(const char* base_url, bool enabled)
    {
        return _request_json(base_url, "/api/audio/input", HTTP_METHOD_PUT,
                             enabled ? "{\"enabled\":true}" : "{\"enabled\":false}");
    }

    OperationResult set_vad(const char* base_url, float rms_min)
    {
        char body[40];
        snprintf(body, sizeof(body), "{\"rms_min\":%.3f}", rms_min);
        return _request_json(base_url, "/api/audio/vad", HTTP_METHOD_PUT, body);
    }

    OperationResult set_backend(const char* base_url, const char* backend)
    {
        char body[40];
        snprintf(body, sizeof(body), "{\"backend\":\"%s\"}", backend);
        return _request_json(base_url, "/api/conversation/backend", HTTP_METHOD_PUT, body);
    }

    OperationResult set_voice(const char* base_url, const char* voice)
    {
        char body[64];
        snprintf(body, sizeof(body), "{\"voice\":\"%s\"}", voice);
        return _request_json(base_url, "/api/conversation/voice", HTTP_METHOD_PUT, body);
    }

    OperationResult restart_yrobot(const char* base_url)
    {
        return _request_json(base_url, "/api/system/restart", HTTP_METHOD_POST, "");
    }
}
