#include "ntp.h"
#include "def.h"

#include <esp_sntp.h>
#include <sys/time.h>
#include <esp_log.h>



void sync_time() {
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int retry = 0;
    const int retry_count = 10;
    
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI("sync_time", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE("sync_time", "Failed to get NTP time.");
    } else {
        ESP_LOGI("sync_time", "NTP time synced successfully.");
        // Set timezone to your local timezone
        setenv("TZ", TIMEZONE, 1);
        tzset();
        
        // Print local time
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI("sync_time", "The current date/time is: %s", strftime_buf);
    }
}

time_t get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

void ntpSyncTime()
{

    // Synchroniser l'heure avec le serveur NTP
    sync_time();

    // Attendre quelques secondes pour permettre la synchronisation
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Obtenir le timestamp actuel
    time_t now = get_timestamp();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Afficher l'heure et la date actuelles
    ESP_LOGI("ntpSyncTime", "Current time: %02d:%02d:%02d %02d/%02d/%04d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}
