/**
 * @file deepseek_client.h
 * @brief One-shot HTTP client for the DeepSeek account balance,
 * proxied through hermes-mcp-xiaozhi. No persistent connection -
 * called once per app open, not continuously polled, same reasoning
 * as STOCK_CLIENT/WEATHER_CLIENT/EMAIL_CLIENT.
 */
#pragma once
#include <string>

namespace DEEPSEEK_CLIENT
{
    struct BalanceInfo
    {
        bool ok = false;
        std::string currency;
        std::string total_balance;
        std::string granted_balance;
        std::string topped_up_balance;
    };

    /**
     * @brief GET {base_url}/api/deepseek/balance. Returns ok=false on
     * any failure.
     */
    BalanceInfo get_balance(const char* base_url);
}
