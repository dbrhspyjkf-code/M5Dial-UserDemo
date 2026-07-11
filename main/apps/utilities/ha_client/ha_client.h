/**
 * @file ha_client.h
 * @brief Thin wrapper around esp_http_client + cJSON for the two Home
 * Assistant REST call shapes this project needs: read a media_player's
 * state, and call a media_player service.
 */
#pragma once
#include <string>

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
}
