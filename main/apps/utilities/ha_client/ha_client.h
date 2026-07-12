/**
 * @file ha_client.h
 * @brief Thin wrapper around esp_http_client + cJSON for the two Home
 * Assistant REST call shapes this project needs: read a media_player's
 * state, and call a media_player service.
 */
#pragma once
#include <string>
#include <vector>

namespace HA_CLIENT
{
    struct MediaPlayerState
    {
        bool ok = false;
        std::string state;   // e.g. "playing", "paused", "idle"
        std::string title;
        std::string artist;
        float volume = 0.0f; // 0.0 - 1.0
    };

    /**
     * @brief GET /api/states/{entity_id}
     */
    MediaPlayerState get_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/{domain}/{service} with body {"entity_id": entity_id}
     */
    bool call_service(const char* base_url, const char* token,
                       const char* domain, const char* service, const char* entity_id);

    /**
     * @brief POST /api/services/media_player/volume_set with
     * {"entity_id": entity_id, "volume_level": volume}
     */
    bool set_volume(const char* base_url, const char* token,
                     const char* entity_id, float volume);

    struct LightState
    {
        bool ok = false;
        bool is_on = false;
        int brightness_pct = 0;                // 0-100
        std::string effect;                    // currently active effect name (may be empty)
        std::vector<std::string> effect_list;  // available effect names (may be empty)
    };

    /**
     * @brief GET /api/states/{entity_id} for a light entity
     */
    LightState get_light_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/light/turn_on with
     * {"entity_id": entity_id, "brightness_pct": brightness_pct}
     */
    bool set_light_brightness(const char* base_url, const char* token,
                               const char* entity_id, int brightness_pct);

    /**
     * @brief POST /api/services/light/turn_on or /turn_off (no extra fields)
     */
    bool set_light_power(const char* base_url, const char* token,
                          const char* entity_id, bool on);

    /**
     * @brief POST /api/services/light/turn_on with
     * {"entity_id": entity_id, "effect": effect_name}
     */
    bool set_light_effect(const char* base_url, const char* token,
                           const char* entity_id, const char* effect_name);
}
