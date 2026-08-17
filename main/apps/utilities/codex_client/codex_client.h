/**
 * @file codex_client.h
 * @brief One-shot HTTP client for the local Codex usage server on the
 * Mac. Follows the same pattern as stock_client — create/destroy the
 * client per call since this is only polled once per minute.
 */
#pragma once
#include <string>

namespace CODEX_CLIENT
{
    struct Usage
    {
        bool ok = false;
        float used = 0.0f;
        float remaining = 0.0f;
        std::string window;
        std::string reset;
    };

    /**
     * @brief GET /codex from the server configured in codex_client_config.h
     */
    Usage fetch(const char* server_url);
}
