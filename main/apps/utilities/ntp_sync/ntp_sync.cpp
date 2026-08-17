/**
 * @file ntp_sync.cpp
 */
#include "ntp_sync.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <ctime>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace NTP_SYNC
{
    static const char* TAG = "ntp_sync";

    static const char* NTP_SERVERS[] = {
        "ntp.aliyun.com",
        "ntp.tencent.com",
        "cn.pool.ntp.org",
        "pool.ntp.org",
        "time.google.com",
    };
    static const int NTP_SERVER_COUNT = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);

    static bool _try_sync_server(const char* server, uint32_t timeout_ms)
    {
        esp_sntp_stop();
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, server);
        esp_sntp_init();

        uint32_t waited_ms = 0;
        sntp_sync_status_t status;
        while ((status = esp_sntp_get_sync_status()) != SNTP_SYNC_STATUS_COMPLETED && waited_ms < timeout_ms)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            waited_ms += 200;
        }

        return (status == SNTP_SYNC_STATUS_COMPLETED);
    }

    void sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms)
    {
        setenv("TZ", "CST-8", 1);
        tzset();

        uint32_t per_server_ms = timeout_ms / NTP_SERVER_COUNT;
        if (per_server_ms < 1000) per_server_ms = 1000;

        for (int i = 0; i < NTP_SERVER_COUNT; i++)
        {
            ESP_LOGI(TAG, "Trying NTP server [%d/%d]: %s", i + 1, NTP_SERVER_COUNT, NTP_SERVERS[i]);
            if (_try_sync_server(NTP_SERVERS[i], per_server_ms))
            {
                time_t now;
                time(&now);
                struct tm timeinfo;
                localtime_r(&now, &timeinfo);

                struct tm rtc_time = timeinfo;
                rtc_time.tm_year = timeinfo.tm_year + 1900;

                hal->rtc.setTime(rtc_time);

                ESP_LOGI(TAG, "RTC synced via %s: %04d-%02d-%02d %02d:%02d:%02d",
                         NTP_SERVERS[i],
                         rtc_time.tm_year, rtc_time.tm_mon + 1, rtc_time.tm_mday,
                         rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
                return;
            }
            ESP_LOGW(TAG, "Server %s timed out", NTP_SERVERS[i]);
        }

        /* NTP failed: seed the SYSTEM clock from the RTC so local time
           is at least plausible (UI reads system time, and the BM8563
           has been observed stopping - so also re-write the registers
           to clear a possible CH halt flag while we are here). */
        struct tm rtc_time;
        if (hal->rtc.getTime(rtc_time) == ESP_OK &&
            rtc_time.tm_hour <= 23 && rtc_time.tm_min <= 59 &&
            rtc_time.tm_mon <= 11 && rtc_time.tm_mday >= 1 && rtc_time.tm_mday <= 31 &&
            rtc_time.tm_year >= 2024)
        {
            struct tm seed = rtc_time;
            seed.tm_year = rtc_time.tm_year - 1900;   /* mktime is 1900-based, hal is 2000-based */
            time_t t = mktime(&seed);
            if (t > (time_t)1700000000)              /* sanity: after ~Nov 2023 */
            {
                struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
                settimeofday(&tv, nullptr);
                ESP_LOGW(TAG, "NTP failed, system clock seeded from RTC");
            }
            /* Rewrite the time registers - clears the CH bit if the
               oscillator had been halted (values are read-then-written
               back, so no visible jump) */
            hal->rtc.setTime(rtc_time);
        }

        ESP_LOGW(TAG, "All %d NTP servers timed out, RTC left unchanged", NTP_SERVER_COUNT);
    }
}
