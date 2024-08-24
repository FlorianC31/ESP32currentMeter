#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gptimer.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_private/esp_clk.h>
#include "nvs_flash.h"
#include <esp_log.h>

#include <vector>
#include <string>

#include "def.h"

static const adc_channel_t ADC_CHANNELS[NB_CHANNELS] = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};


typedef struct {
    uint16_t index;
    int64_t timestamp;
    int value;
} adc_reading_t;

std::vector<adc_reading_t> buffer1;
std::vector<adc_reading_t> buffer2;
std::vector<adc_reading_t>* currentBuffer(&buffer1);
std::vector<adc_reading_t>* lastBuffer(&buffer2);

uint16_t sampleIndex = 0;

QueueHandle_t adc_queue;
adc_oneshot_unit_handle_t adc1_handle;
TaskHandle_t process_task_handle = NULL;
gptimer_handle_t gptimer = NULL;

// Function prototype for real-time processing
void process_adc_sample(const adc_reading_t& reading);

static bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data) {
    BaseType_t high_task_awoken = pdFALSE;
    
    int adc_raw;
    adc_oneshot_read(adc1_handle, ADC_CHANNELS[TENSION_ID], &adc_raw);
    
    adc_reading_t reading = {
        .index = sampleIndex,
        .timestamp = esp_timer_get_time(),
        .value = adc_raw
    };

    sampleIndex++;
    
    xQueueSendFromISR(adc_queue, &reading, &high_task_awoken);
    
    return high_task_awoken == pdTRUE;
}

void adc_task(void *pvParameters) {
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNELS[TENSION_ID], &config));

    // Initialize timer
    gptimer_config_t timer_config;
    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_config.direction = GPTIMER_COUNT_UP;
    timer_config.resolution_hz = 1000000; // 1MHz, 1 tick = 1us
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_alarm_config_t alarm_config;
    alarm_config.alarm_count = TIM_PERIOD;
    alarm_config.reload_count = 0;
    alarm_config.flags.auto_reload_on_alarm = true;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    adc_reading_t reading;
    while(1) {
        if (xQueueReceive(adc_queue, &reading, portMAX_DELAY)) {
            process_adc_sample(reading);
        }
    }
}

// This is where you implement your real-time processing logic
void process_adc_sample(const adc_reading_t& reading) {

    currentBuffer->push_back(reading);
    if (currentBuffer->size() == NB_SAMPLES) {
        std::vector<adc_reading_t>* tempBuffer(currentBuffer);
        currentBuffer = lastBuffer;
        lastBuffer = tempBuffer;
        currentBuffer->clear();
        xTaskNotifyGive(process_task_handle);
    }

}


void processing_task(void *pvParameters) {
    while(1) {
        // Wait for notification from process_adc_sample
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        std::string indexList("Index");
        std::string timestampList("TimeStamps");
        std::string valueList("Values");

        for (const auto &adcReading : *lastBuffer) {
            indexList += ";" + std::to_string(adcReading.index);
            timestampList += ";" + std::to_string(adcReading.timestamp);
            valueList += ";" + std::to_string(adcReading.value);
        }

        ESP_LOGI("ADC", "%s", indexList.c_str());
        ESP_LOGI("ADC", "%s", timestampList.c_str());
        ESP_LOGI("ADC", "%s", valueList.c_str());
    }
}


extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    buffer1.reserve(NB_SAMPLES);
    buffer2.reserve(NB_SAMPLES);
    
    adc_queue = xQueueCreate(10, sizeof(adc_reading_t));  // Small queue size for real-time processing
    
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(processing_task, "Processing Task", 4096, NULL, 1, &process_task_handle, 1);
}