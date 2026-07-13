/**
 * @file weather_client.h
 * @brief One-shot HTTP client for the hermes-mcp-xiaozhi weather
 * endpoint. Same non-persistent-connection reasoning as STOCK_CLIENT -
 * low call frequency (once per screensaver activation), no auth
 * needed, small response.
 */
#pragma once
#include <string>

namespace WEATHER_CLIENT
{
    struct WeatherInfo
    {
        bool ok = false;
        std::string temp_c;
        std::string condition;
    };

    /**
     * @brief GET {base_url}/weather (default city, already configured
     * server-side). Returns ok=false on any failure.
     */
    WeatherInfo get_weather(const char* base_url);
}
