#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_adc/adc_continuous.h>
#include <driver/gpio.h>
#include "nvs_flash.h"
#include <esp_log.h>

#include <array>
#include <vector>
#include <string>

#define NB_SAMPLES      128
#define ANALYZED_PERIOD 20.480  // ms (A little bit more than 1/50Hz = 20ms)
#define NB_CURRENTS     5
#define NB_CHANNELS     (NB_CURRENTS + 2)
#define SAMPLE_FREQ     (1000 * NB_SAMPLES / ANALYZED_PERIOD)  // 6.25kHz
#define TENSION_ID      (NB_CURRENTS + 0)
#define VREF_ID         (NB_CURRENTS + 1)

// Adjusted ADC buffer size for better performance
#define ADC_BUFFER_SIZE (NB_CHANNELS * NB_SAMPLES * SOC_ADC_DIGI_RESULT_BYTES)


static const char* TAG = "ADC_APP";

static const std::array<adc_channel_t, NB_CHANNELS> ADC_CHANNELS = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};

struct adc_reading_t {
    uint16_t index;
    uint32_t timestamp;
    std::array<uint32_t, NB_CHANNELS> values;
};

static SemaphoreHandle_t bufferMutex = NULL;
//static QueueHandle_t bufferReadyQueue = NULL;

std::array<std::array<uint32_t, NB_SAMPLES>, NB_CHANNELS> adcBuffer;


static adc_continuous_handle_t adc_handle = NULL;
static TaskHandle_t adc_task_handle = NULL;
//static TaskHandle_t process_task_handle = NULL;

uint32_t lastTimestamp = 0;


/**
 * @brief Callback function for ADC conversion events
 * 
 * This function is called by the ADC driver when a batch of ADC conversions is completed.
 * It keeps track of the number of samples collected across all channels.
 * When the desired number of samples (NB_SAMPLES) has been reached for each channel,
 * it notifies the ADC task that a complete set of data is ready for processing.
 * 
 * @param handle The handle of the ADC continuous mode driver
 * @param edata Pointer to the event data, containing information about the completed conversions
 * @param user_data User data passed when registering the callback (unused in this implementation)
 * 
 * @return true if a higher priority task was woken by the notification, false otherwise
 */
static bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    static uint32_t sample_count = 0;
    sample_count += edata->size / (SOC_ADC_DIGI_RESULT_BYTES * NB_CHANNELS);
    
    if (sample_count >= NB_SAMPLES) {
        sample_count = 0;
        BaseType_t high_task_awoken = pdFALSE;
        vTaskNotifyGiveFromISR(adc_task_handle, &high_task_awoken);
        return high_task_awoken == pdTRUE;
    }
    return false;
}

/**
 * @brief ADC task function
 * 
 * This task configures the ADC, starts continuous sampling, and processes the incoming data.
 */
void adc_task(void *pvParameters) {
    ESP_LOGI(TAG, "ADC task starting");
    
    esp_err_t ret;
    uint32_t ret_num = 0;
    std::array<uint8_t, ADC_BUFFER_SIZE> adc_raw;

    adc_continuous_handle_cfg_t adc_config;
    adc_config.max_store_buf_size = ADC_BUFFER_SIZE;
    adc_config.conv_frame_size = SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NB_CHANNELS;
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg;
    dig_cfg.sample_freq_hz = static_cast<uint32_t>(SAMPLE_FREQ * NB_CHANNELS);
    dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;

    std::vector<adc_digi_pattern_config_t> adc_pattern(NB_CHANNELS);
    for (int i = 0; i < NB_CHANNELS; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = ADC_CHANNELS[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    dig_cfg.pattern_num = NB_CHANNELS;
    dig_cfg.adc_pattern = adc_pattern.data();
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    adc_continuous_evt_cbs_t cbs;
    cbs.on_conv_done = adc_conv_done_cb;
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint32_t curentTimestamp = esp_timer_get_time();
        if (lastTimestamp != 0) {
            uint32_t detlaT = (curentTimestamp - lastTimestamp);
            ESP_LOGW(TAG, "Actual Period : %luµs", detlaT);
        }
        lastTimestamp = curentTimestamp;


        ret = adc_continuous_read(adc_handle, adc_raw.data(), adc_raw.size(), &ret_num, 0);
        if (ret == ESP_OK) {
            if (ret_num == ADC_BUFFER_SIZE) {
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = reinterpret_cast<adc_digi_output_data_t*>(&adc_raw[i]);

                    uint16_t bufferIndex = i / (NB_CHANNELS * SOC_ADC_DIGI_RESULT_BYTES);

                    if (p->type2.channel < NB_CHANNELS) {
                        if (p->type2.channel >= NB_CHANNELS || bufferIndex >= NB_SAMPLES) {
                            ESP_LOGW(TAG, "Buffer index error: %u/%i - %u/%i", p->type2.channel, NB_CHANNELS, bufferIndex, NB_SAMPLES);
                        }

                        xSemaphoreTake(bufferMutex, portMAX_DELAY);     
                        adcBuffer[p->type2.channel][bufferIndex] = p->type2.data;
                        xSemaphoreGive(bufferMutex);
                    } else {
                        ESP_LOGW(TAG, "Error unknown channel: %u", p->type2.channel);
                    }
                }
            } else {
                ESP_LOGW(TAG, "Error on ADC buffer size: %lub - Expected: %ib", ret_num, ADC_BUFFER_SIZE);
            }
        } else {
            ESP_LOGW(TAG, "ADC continuous read failed: %s", esp_err_to_name(ret));
        }
    }
}

/**
 * @brief Process and log task function
 * 
 * This task processes the ADC data and logs the results.
 */
/*void process_and_log_task(void *pvParameters) {
    ESP_LOGI(TAG, "Process and log task starting");
    std::array<adc_reading_t, NB_SAMPLES>* buffer_to_process;
    uint32_t processed_count = 0;

    while (1) {
        if (xQueueReceive(bufferReadyQueue, &buffer_to_process, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(bufferMutex, portMAX_DELAY);
            
            std::string indexValues = "Index Values";
            std::string timestampValues = "TimeStamp Values";
            std::string tensionValues = "Tension Values";
            std::string vrefValues = "Vref Values";

            for (const auto& sample : *buffer_to_process) {
                indexValues += ";" + std::to_string(sample.index);
                timestampValues += ";" + std::to_string(sample.timestamp);
                tensionValues += ";" + std::to_string(sample.values[TENSION_ID]);
                vrefValues += ";" + std::to_string(sample.values[VREF_ID]);
            }

            processed_count += NB_SAMPLES;
            ESP_LOGI(TAG, "Processed %lu samples", processed_count);
            ESP_LOGI(TAG, "Last value of buffer %luV", (*buffer_to_process)[NB_SAMPLES-1].values[TENSION_ID]);
            ESP_LOGI(TAG, "Last timestamp of buffer %lu", (*buffer_to_process)[NB_SAMPLES-1].timestamp);

            ESP_LOGI(TAG, "%s", indexValues.c_str());
            ESP_LOGI(TAG, "%s", timestampValues.c_str());
            ESP_LOGI(TAG, "%s", tensionValues.c_str());
            ESP_LOGI(TAG, "%s", vrefValues.c_str());

            int64_t time_diff = (*buffer_to_process)[NB_SAMPLES-1].timestamp - (*buffer_to_process)[0].timestamp;
            float actual_rate = (NB_SAMPLES - 1) * 1000000.0f / time_diff;
            ESP_LOGI(TAG, "Actual sampling rate: %.2f Hz", actual_rate);

            xSemaphoreGive(bufferMutex);
        }
    }
}*/

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    bufferMutex = xSemaphoreCreateMutex();
    //bufferReadyQueue = xQueueCreate(2, sizeof(std::array<adc_reading_t, NB_SAMPLES>*));

    //xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 1);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 0);

    ESP_LOGI(TAG, "Tasks created, application running");
}