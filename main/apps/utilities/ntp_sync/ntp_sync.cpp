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

    void sync_rtc_time(HAL::HAL* hal, uint32_t timeout_ms)
    {
        setenv("TZ", "CST-8", 1);
        tzset();

        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_init();

        /* SNTP_SYNC_STATUS_COMPLETED is transient - the library resets
           it back to SNTP_SYNC_STATUS_RESET shortly after it's first
           observed, so the status must be captured at the exact check
           that breaks this loop, never re-queried afterward (a second
           query would very likely see it already reset and wrongly
           report failure even though the sync succeeded). */
        uint32_t waited_ms = 0;
        sntp_sync_status_t status;
        while ((status = esp_sntp_get_sync_status()) != SNTP_SYNC_STATUS_COMPLETED && waited_ms < timeout_ms)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            waited_ms += 200;
        }

        if (status != SNTP_SYNC_STATUS_COMPLETED)
        {
            ESP_LOGW(TAG, "NTP sync timed out after %u ms, RTC left unchanged", (unsigned)waited_ms);
            return;
        }

        time_t now;
        time(&now);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        /* PCF8563::setTime() expects tm_year as the actual full year
           (e.g. 2026), not POSIX's "years since 1900" - localtime_r()
           gives us the POSIX convention, so it must be adjusted here. */
        struct tm rtc_time = timeinfo;
        rtc_time.tm_year = timeinfo.tm_year + 1900;

        hal->rtc.setTime(rtc_time);

        ESP_LOGI(TAG, "RTC synced: %04d-%02d-%02d %02d:%02d:%02d",
                 rtc_time.tm_year, rtc_time.tm_mon + 1, rtc_time.tm_mday,
                 rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
    }
}
