/**
 * @file stock_client.cpp
 */
#include "stock_client.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

namespace STOCK_CLIENT
{
    static const char* TAG = "stock_client";

    struct ResponseBuffer
    {
        char* data;
        int len;
        int capacity;
    };

    static esp_err_t _http_event_handler(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA)
        {
            auto* buf = (ResponseBuffer*)evt->user_data;
            int space = buf->capacity - buf->len - 1;
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

    std::vector<StockItem> get_portfolio(const char* base_url)
    {
        std::vector<StockItem> result;

        char url[128];
        snprintf(url, sizeof(url), "%s/api/stocks/portfolio", base_url);

        const int BUF_CAPACITY = 16384;
        ResponseBuffer resp_buf;
        resp_buf.data = (char*)malloc(BUF_CAPACITY);
        resp_buf.len = 0;
        resp_buf.capacity = BUF_CAPACITY;
        if (resp_buf.data == nullptr)
        {
            ESP_LOGE(TAG, "get_portfolio: malloc failed");
            return result;
        }
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
            ESP_LOGE(TAG, "get_portfolio failed: err=%d status=%d", err, status);
            free(resp_buf.data);
            return result;
        }

        cJSON* root = cJSON_Parse(resp_buf.data);
        free(resp_buf.data);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "get_portfolio: JSON parse failed");
            return result;
        }

        cJSON* items = cJSON_GetObjectItem(root, "items");
        if (cJSON_IsArray(items))
        {
            int count = cJSON_GetArraySize(items);
            for (int i = 0; i < count; i++)
            {
                cJSON* item = cJSON_GetArrayItem(items, i);
                if (item == nullptr) continue;

                StockItem stock;

                cJSON* code = cJSON_GetObjectItem(item, "code");
                if (cJSON_IsString(code)) stock.code = code->valuestring;

                cJSON* name = cJSON_GetObjectItem(item, "name");
                if (cJSON_IsString(name)) stock.name = name->valuestring;

                cJSON* price = cJSON_GetObjectItem(item, "price");
                if (cJSON_IsNumber(price)) stock.price = (float)price->valuedouble;

                cJSON* chg = cJSON_GetObjectItem(item, "chg");
                if (cJSON_IsNumber(chg)) stock.chg = (float)chg->valuedouble;

                result.push_back(stock);
            }
        }

        cJSON_Delete(root);
        return result;
    }
}
