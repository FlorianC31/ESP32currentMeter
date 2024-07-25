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
#include <esp_adc/adc_continuous.h>

#define WIFI_SSID "Livebox-Florelie"
#define WIFI_PASS "r24hpkr2"

#define NUM_CHANNELS 7
#define SAMPLE_RATE 1000
#define DMA_BUFFER_SIZE (1024 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV)

static const adc_channel_t ADC_CHANNELS[NUM_CHANNELS] = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};

static volatile int adcValues[NUM_CHANNELS] = {0};
static volatile bool actionFlag = false;
static SemaphoreHandle_t mutex;
static uint8_t *adc_raw;
static adc_continuous_handle_t adc_handle = NULL;

static int64_t maxTime = 0;
static int64_t minTime = 1000000;
static int64_t meanTime = 0;
static const int64_t nbSamples = 1000;
static int s = 0;

static bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    BaseType_t high_task_awoken = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(user_data), &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

static void adc_timer_callback(void* arg) {
    int64_t start_time = esp_timer_get_time();
    BaseType_t high_task_awoken = pdFALSE;
   
    for (int i = 0; i < NUM_CHANNELS; i++) {
        xSemaphoreTakeFromISR(mutex, &high_task_awoken);
        uint32_t adc_reading = ((uint32_t*)adc_raw)[i];
        adcValues[i] = adc_reading & 0xFFF;
        xSemaphoreGiveFromISR(mutex, &high_task_awoken);
    }
   
    if (high_task_awoken) {
        portYIELD_FROM_ISR();
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
}

static void adc_task(void *pvParameters) {
    esp_err_t ret;
    
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = DMA_BUFFER_SIZE,
        .conv_frame_size = SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NUM_CHANNELS,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg;
    dig_cfg.sample_freq_hz = SAMPLE_RATE * NUM_CHANNELS;
    dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;

    adc_digi_pattern_config_t adc_pattern[NUM_CHANNELS];
    for (int i = 0; i < NUM_CHANNELS; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_11;
        adc_pattern[i].channel = ADC_CHANNELS[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    dig_cfg.pattern_num = NUM_CHANNELS;
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    adc_raw = static_cast<uint8_t*>(heap_caps_malloc(DMA_BUFFER_SIZE, MALLOC_CAP_DMA));
    assert(adc_raw != NULL);

    adc_continuous_evt_cbs_t cbs;
    cbs.on_conv_done = adc_conv_done_cb;

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, xTaskGetCurrentTaskHandle()));
    
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    esp_timer_create_args_t timer_args;
    timer_args.callback = &adc_timer_callback;
    timer_args.name = "adc_timer";
   
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000000 / SAMPLE_RATE));
   
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint32_t ret_num = 0;
        ret = adc_continuous_read(adc_handle, adc_raw, DMA_BUFFER_SIZE, &ret_num, 0);
        if (ret == ESP_OK) {
            // Process data here if needed
        }
        
        vTaskDelay(1);
    }
}

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