/**
 * @file email_client.h
 * @brief One-shot HTTP client for the user's email-status-server
 * (part of hermes-mcp-xiaozhi). No persistent connection - called
 * once per app open, not continuously polled, same reasoning as
 * STOCK_CLIENT/WEATHER_CLIENT.
 */
#pragma once
#include <string>
#include <vector>

namespace EMAIL_CLIENT
{
    struct FolderInfo
    {
        std::string name;
        int unread = 0;
        std::string latest_subject;
        std::string latest_from;
    };

    /**
     * @brief GET {base_url}/api/email/status, parse the "folders" array
     * (only folders with unread > 0 are ever included by the server).
     * Returns an empty vector on any failure.
     */
    std::vector<FolderInfo> get_unread(const char* base_url);
}
