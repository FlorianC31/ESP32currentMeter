
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_adc/adc_continuous.h>

#include <string>
#include <vector>
#include <array>

#include "adc.h"
//#include "measure.h"


static adc_continuous_handle_t adc_handle = NULL;
TaskHandle_t adc_task_handle = NULL;
SemaphoreHandle_t bufferMutex = NULL;

std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* inputBuffer = nullptr;
std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* outputBuffer = nullptr;

//Chrono adcChrono("Adc", 20.5, 500);

extern const std::array<adc_channel_t, NB_CHANNELS> ADC_CHANNELS = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};


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
    const char* TAG = "ADC_APP";

    ESP_LOGI(TAG, "ADC task starting");

    
    bufferMutex = xSemaphoreCreateMutex();
    
    esp_err_t ret;
    uint32_t ret_num = 0;

    adc_continuous_handle_cfg_t adc_config;
    adc_config.max_store_buf_size = ADC_BUFFER_SIZE;
    adc_config.conv_frame_size = SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NB_CHANNELS;
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg;
    dig_cfg.sample_freq_hz = static_cast<uint32_t>(SAMPLE_FREQ * NB_CHANNELS);
    dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;

    std::array<adc_digi_pattern_config_t, NB_CHANNELS> adc_pattern;
    ESP_LOGI(TAG, "Flag2.1");
    for (int i = 0; i < NB_CHANNELS; i++) {
        ESP_LOGI(TAG, "Channel %i", i);
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = ADC_CHANNELS[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    ESP_LOGI(TAG, "Flag2.1");
    dig_cfg.pattern_num = NB_CHANNELS;
    ESP_LOGI(TAG, "Flag2.2");
    dig_cfg.adc_pattern = adc_pattern.data();
    ESP_LOGI(TAG, "Flag2.3");
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    
    ESP_LOGI(TAG, "Flag3");

    adc_continuous_evt_cbs_t cbs;
    cbs.on_conv_done = adc_conv_done_cb;
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    
    ESP_LOGI(TAG, "Flag4");

    std::array<uint8_t, ADC_BUFFER_SIZE> adc_raw;
    std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> buffer1;
    std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> buffer2;
    inputBuffer = &buffer1;
    outputBuffer = &buffer2;

    //bufferMutex = xSemaphoreCreateMutex();


    ESP_LOGI(TAG, "Flag5");

    while (1) {
        ESP_LOGI(TAG, "Waiting notif");
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Notif received");

        //adcChrono.startCycle();

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
   
                        (*inputBuffer)[bufferIndex][p->type2.channel] = p->type2.data;
                    } else {
                        ESP_LOGW(TAG, "Error unknown channel: %u", p->type2.channel);
                    }
                }
                xSemaphoreTake(bufferMutex, portMAX_DELAY);
                std::swap(inputBuffer, outputBuffer);
                xSemaphoreGive(bufferMutex);
                //xTaskNotifyGive(process_task_handle);

            } else {
                ESP_LOGW(TAG, "Error on ADC buffer size: %lub - Expected: %ib", ret_num, ADC_BUFFER_SIZE);
            }
        } else {
            ESP_LOGW(TAG, "ADC continuous read failed: %s", esp_err_to_name(ret));
        }
        ESP_LOGI(TAG, "First loop ended");
        
        //adcChrono.endCycle();
    }
}
