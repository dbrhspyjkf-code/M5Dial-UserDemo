/**
 * @file wifi_connect_wrap.h
 * @brief Reusable, blocking WiFi STA connect/disconnect. Refactored from
 * the working patterns already in wifi_factory_test.c (connect) and
 * wifi_common_test.cpp (teardown), parameterized instead of hardcoded.
 */
#pragma once
#include <cstdint>

namespace WIFI_CONNECT
{
    /**
     * @brief Connect to a WiFi AP, blocking until connected or timed out.
     *
     * @param ssid
     * @param password
     * @param timeout_ms
     * @return true if connected, false on failure/timeout
     */
    bool connect(const char* ssid, const char* password, uint32_t timeout_ms);

    /**
     * @brief Tear down WiFi fully (stop, deinit, destroy netif, delete
     * event loop, deinit NVS). Safe to call only after a successful connect().
     */
    void disconnect();
}
