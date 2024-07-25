#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <cJSON.h>
#include "adc.h"


#define WIFI_SSID "Livebox-Florelie"
#define WIFI_PASS "r24hpkr2"




/*static esp_err_t get_adc_data_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key[8];
        snprintf(key, sizeof(key), "adc%d", i);
        cJSON_AddNumberToObject(root, key, adcValues[i]);
    }
    xSemaphoreGive(mutex);
    
    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t trigger_action_handler(httpd_req_t *req) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    actionFlag = true;
    xSemaphoreGive(mutex);
    
    httpd_resp_send(req, "Action triggered", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri      = "/api/data",
            .method   = HTTP_GET,
            .handler  = get_adc_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);
        
        httpd_uri_t uri_post = {
            .uri      = "/api/action",
            .method   = HTTP_POST,
            .handler  = trigger_action_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);
    }
    
    return server;
}*/

/*static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        start_webserver();
    }
}*/

/*static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    
    wifi_config_t wifi_config;
    wifi_config.sta.ssid = WIFI_SSID;
    wifi_config.sta.password = WIFI_PASS;
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.bssid_set = false;
    wifi_config.sta.channel = 0;
    wifi_config.sta.listen_interval = 0;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}*/

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    mutex = xSemaphoreCreateMutex();
    
    //wifi_init_sta();
    
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 4096, NULL, 5, NULL, 0);
}