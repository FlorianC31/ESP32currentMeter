#include "ntp.h"

#include <esp_sntp.h>
#include <sys/time.h>
#include <esp_log.h>

#include "esp_ping.h"
#include "lwip/inet.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"

static const char *TAG = "NTP";

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}


/*void sync_time()
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}*/


bool check_internet_connection() {
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    
    // Convert IP address to ip_addr_t
    if (ipaddr_aton("8.8.8.8", &target_addr) == 0) {
        log_e("Invalid IP address");
        return false;
    }

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 3; // Number of pings to perform

    esp_ping_handle_t ping_handle;
    if (esp_ping_new_session(&ping_config, NULL, &ping_handle) != ESP_OK) {
        log_e("Failed to create ping session");
        return false;
    }

    esp_ping_start(ping_handle);

    // Wait for ping to complete
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    uint32_t received = 0;
    uint32_t transmitted = 0;
    esp_ping_get_profile(ping_handle, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(ping_handle, ESP_PING_PROF_REPLY, &received, sizeof(received));

    esp_ping_delete_session(ping_handle);

    if (received > 0) {
        log_i("Internet connection available");
        return true;
    } else {
        log_e("No internet connection");
        return false;
    }
}

void sync_time() {
    check_internet_connection();
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int retry = 0;
    const int retry_count = 10;
    
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE(TAG, "Failed to get NTP time.");
    } else {
        ESP_LOGI(TAG, "NTP time synced successfully.");
        // Set timezone to your local timezone
        setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
        tzset();
        
        // Print local time
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
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
    ESP_LOGI(TAG, "Current time: %02d:%02d:%02d %02d/%02d/%04d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}
