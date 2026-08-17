/**
 * @file reachy_client.h
 * @brief Minimal YRobot chat-log client for the M5Dial Reachy app.
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace REACHY_CLIENT
{
    struct OperationResult
    {
        bool ok = false;
        int status = 0;
    };

    struct AudioState
    {
        bool ok = false;
        int volume_percent = -1;
        bool mic_enabled = true;
        float vad_rms_min = 0.05f;
    };

    struct VideoState
    {
        bool ok = false;
        bool enabled = false;
        float active_s = 0.0f;
        float idle_s = 0.0f;
    };

    struct ChatTurn
    {
        bool ok = false;
        std::string user;
        std::string assistant;
    };

    struct JpegFrame
    {
        bool ok = false;
        int status = 0;
        std::vector<uint8_t> bytes;
    };

    struct ModeState
    {
        bool ok = false;
        std::string configured_backend = "xiaozhi";
        std::string running_backend = "xiaozhi";
        std::string configured_voice;
        std::vector<std::string> available_voices;
    };

    ChatTurn fetch_recent_turn(const char* base_url, int limit = 200);
    AudioState fetch_audio_state(const char* base_url);
    VideoState fetch_video_state(const char* base_url);
    bool prepare_camera_jpeg_buffer(JpegFrame& frame);
    JpegFrame fetch_camera_jpeg(const char* base_url);
    bool fetch_camera_jpeg(const char* base_url, JpegFrame& frame);
    ModeState fetch_mode_state(const char* base_url);
    OperationResult set_volume(const char* base_url, int percent);
    OperationResult set_mic_enabled(const char* base_url, bool enabled);
    OperationResult set_vad(const char* base_url, float rms_min);
    OperationResult set_video_enabled(const char* base_url, bool enabled);
    OperationResult set_backend(const char* base_url, const char* backend);
    OperationResult set_voice(const char* base_url, const char* voice);
    OperationResult restart_yrobot(const char* base_url);
}
