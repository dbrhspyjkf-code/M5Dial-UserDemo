/**
 * @file weather_client.h
 * @brief One-shot HTTP client for the hermes-mcp-xiaozhi weather
 * endpoint. Same non-persistent-connection reasoning as STOCK_CLIENT -
 * low call frequency (screensaver poll), no auth needed, small response.
 */
#pragma once
#include <string>

namespace WEATHER_CLIENT
{
    struct WeatherInfo
    {
        bool ok = false;
        std::string city;           /* "广州" */
        std::string temp_c;         /* "27.0" */
        std::string feels_like_c;   /* "29.0" */
        std::string humidity;       /* "93" (%), no unit in payload */
        std::string condition;      /* "雨" (Chinese, from server) */
        std::string condition_code; /* "rainy" (machine code) */
    };

    /**
     * @brief GET {base_url}/weather (default city, already configured
     * server-side). Returns ok=false on any failure.
     */
    WeatherInfo get_weather(const char* base_url);
}
