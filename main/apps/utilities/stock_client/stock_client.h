/**
 * @file stock_client.h
 * @brief One-shot HTTP client for the user's hermes-mcp-xiaozhi stock
 * watchlist API. No persistent connection (unlike HA_CLIENT) - this
 * endpoint is only ever called once per app open, not continuously
 * polled, and its response body (~12KB, includes analysis text this
 * app doesn't even display) is far larger than HA_CLIENT's fixed
 * 2048-byte buffer, so a fresh malloc'd buffer per call is simpler
 * and correct at this call frequency.
 */
#pragma once
#include <string>
#include <vector>

namespace STOCK_CLIENT
{
    struct StockItem
    {
        std::string code;
        std::string name;
        float price = 0.0f;
        float chg = 0.0f;      // percentage change, e.g. 5.34 means +5.34%
        float abs_chg = 0.0f;  // absolute change in price (yuan), from the API's "pchg" field
    };

    /**
     * @brief GET {base_url}/api/stocks/portfolio, parse the "items"
     * array. Returns an empty vector on any failure (unreachable,
     * non-200, JSON parse failure, malloc failure).
     */
    std::vector<StockItem> get_portfolio(const char* base_url);
}
