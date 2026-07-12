/**
 * @file ha_client.h
 * @brief Thin wrapper around esp_http_client + cJSON for the Home
 * Assistant REST calls this project needs. Uses one persistent,
 * mutex-guarded esp_http_client connection for the device's whole
 * lifetime (instead of creating/destroying a client per call) - avoids
 * the ~400 byte internal-heap loss per call that previously
 * accumulated every time an HA app or RFID_SERVICE made a request,
 * eventually causing "unreachable" connect timeouts under memory
 * pressure. The mutex also makes it safe to call from RFID_SERVICE's
 * background task and an open app's task at the same time.
 */
#pragma once
#include <string>
#include <vector>

namespace HA_CLIENT
{
    /**
     * @brief Create the shared client and mutex. Call exactly once, at
     * boot, before any other HA_CLIENT function or RFID_SERVICE::init()
     * (RFID_SERVICE's scan handler calls into HA_CLIENT too).
     */
    void init();

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

    struct SwitchState
    {
        bool ok = false;
        bool is_on = false;
    };

    /**
     * @brief GET /api/states/{entity_id} for a switch entity
     */
    SwitchState get_switch_state(const char* base_url, const char* token, const char* entity_id);

    struct NumberState
    {
        bool ok = false;
        float value = 0.0f;
        float min = 0.0f;
        float max = 100.0f;
    };

    /**
     * @brief GET /api/states/{entity_id} for a number entity. Unlike other
     * domains, HA's top-level "state" field for number entities is the
     * value itself (a numeric string), not "on"/"off".
     */
    NumberState get_number_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/number/set_value with
     * {"entity_id": entity_id, "value": value}
     */
    bool set_number_value(const char* base_url, const char* token,
                           const char* entity_id, float value);

    struct FanState
    {
        bool ok = false;
        bool is_on = false;
        int percentage = 0;      // 0-100
        bool oscillating = false;
    };

    /**
     * @brief GET /api/states/{entity_id} for a fan entity
     */
    FanState get_fan_state(const char* base_url, const char* token, const char* entity_id);

    /**
     * @brief POST /api/services/fan/turn_on or /turn_off (no extra fields)
     */
    bool set_fan_power(const char* base_url, const char* token,
                        const char* entity_id, bool on);

    /**
     * @brief POST /api/services/fan/set_percentage with
     * {"entity_id": entity_id, "percentage": percentage}
     */
    bool set_fan_percentage(const char* base_url, const char* token,
                             const char* entity_id, int percentage);

    /**
     * @brief POST /api/services/fan/oscillate with
     * {"entity_id": entity_id, "oscillating": true/false}
     */
    bool set_fan_oscillating(const char* base_url, const char* token,
                              const char* entity_id, bool oscillating);
}
