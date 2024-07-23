#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "cJSON.h"

#define WIFI_SSID "Livebox-Florelie"
#define WIFI_PASS "r24hpkr2"

#define NUM_CHANNELS 7
#define SAMPLE_RATE 1000 //6250

static const adc_channel_t ADC_CHANNELS[NUM_CHANNELS] = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};

static volatile int adcValues[NUM_CHANNELS] = {0};
static volatile bool actionFlag = false;
static SemaphoreHandle_t mutex;
static adc_oneshot_unit_handle_t adc1_handle;

static int64_t maxTime = 0;
static int64_t minTime = 1000000;
static int64_t meanTime = 0;

static const int64_t nbSamples = 1000;
static volatile int s = 0;

static void adc_timer_callback(void* arg) {
    //ESP_LOGI("ADC", "Reading ADC");
    //gpio_set_level(GPIO_NUM_6, 1);
    int64_t start_time = esp_timer_get_time();
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        int adc_reading;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNELS[i], &adc_reading));
        xSemaphoreTake(mutex, 0);
        adcValues[i] = adc_reading;
        xSemaphoreGive(mutex);
    }
    
    int64_t end_time = esp_timer_get_time();
    int64_t adc_conversion_time = end_time - start_time;

    if (adc_conversion_time > maxTime) {
        maxTime = adc_conversion_time;
    }
    if (adc_conversion_time < minTime) {
        minTime = adc_conversion_time;
    }
    meanTime = (meanTime * s + adc_conversion_time) / (s + 1);
    s++;
    if (s == nbSamples) {
        ESP_LOGI("ADC", "min time: %lld µs - max time: %lld µs - mean time: %lld µs", minTime, maxTime, meanTime);
        s = 0;
        maxTime = 0;
        minTime = 1000000;
        meanTime = 0;
    }


    //gpio_set_level(GPIO_NUM_6, 0);
}

static void adc_task(void *pvParameters) {
    gpio_set_direction(GPIO_NUM_6, GPIO_MODE_OUTPUT);
    
    adc_oneshot_unit_init_cfg_t init_config1;
    init_config1.unit_id = ADC_UNIT_1;

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    for (int i = 0; i < NUM_CHANNELS; i++) {
        adc_oneshot_chan_cfg_t config;
        config.bitwidth = ADC_BITWIDTH_12;
        config.atten = ADC_ATTEN_DB_11;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNELS[i], &config));
    }
    
    esp_timer_create_args_t timer_args;
    timer_args.callback = &adc_timer_callback;
    timer_args.name = "adc_timer";
    timer_args.arg = NULL;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.skip_unhandled_events = false;
    
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000000 / (SAMPLE_RATE)));
    
    while(1) {
        vTaskDelay(portMAX_DELAY);
    }
}

static esp_err_t get_adc_data_handler(httpd_req_t *req) {
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
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
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
}

static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .scan_method = WIFI_FAST_SCAN,
            .bssid_set = false,
            .channel = 0,
            .listen_interval = 0,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    mutex = xSemaphoreCreateMutex();
    
    wifi_init_sta();
    
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 4096, NULL, 5, NULL, 0);
}