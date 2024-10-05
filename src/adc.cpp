#include "adc.h"
#include "chrono.h"

// Adjusted ADC buffer size for better performance
#define ADC_BUFFER_SIZE (NB_CHANNELS * NB_SAMPLES * SOC_ADC_DIGI_RESULT_BYTES)


static const char* TAG = "ADC_TASK";

static const std::array<adc_channel_t, NB_CHANNELS> ADC_CHANNELS = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};

struct adc_reading_t {
    uint16_t index;
    uint32_t timestamp;
    std::array<uint32_t, NB_CHANNELS> values;
};

SemaphoreHandle_t bufferMutex = NULL;
//static QueueHandle_t bufferReadyQueue = NULL;

std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> buffer1;
std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> buffer2;

std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* inputBuffer(&buffer1);
std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* outputBuffer(&buffer2);


static adc_continuous_handle_t adc_handle = NULL;
TaskHandle_t adc_task_handle = NULL;

uint32_t lastTimestampAdc = 0;


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

    //Chrono chrono("Adc", 20.5, 20);


    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint32_t curentTimestamp = esp_timer_get_time();
        if (lastTimestampAdc != 0) {
            uint32_t detlaT = (curentTimestamp - lastTimestampAdc);
            ESP_LOGI(TAG, "adc_task - Actual Period : %luµs", detlaT);
        }
        lastTimestampAdc = curentTimestamp;

        //chrono.startCycle();

        ret = adc_continuous_read(adc_handle, adc_raw.data(), adc_raw.size(), &ret_num, 0);
        if (ret == ESP_OK) {
            if (ret_num == ADC_BUFFER_SIZE) {
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = reinterpret_cast<adc_digi_output_data_t*>(&adc_raw[i]);

                    uint16_t bufferIndex = i / (NB_CHANNELS * SOC_ADC_DIGI_RESULT_BYTES);

                    if (p->type2.channel < NB_CHANNELS) {
                        if (p->type2.channel >= NB_CHANNELS || bufferIndex >= NB_SAMPLES) {
                            ESP_LOGE(TAG, "Buffer index error: %u/%i - %u/%i", p->type2.channel, NB_CHANNELS, bufferIndex, NB_SAMPLES);
                        }
   
                        (*inputBuffer)[bufferIndex][p->type2.channel] = p->type2.data;
                    } else {
                        ESP_LOGE(TAG, "Error unknown channel: %u", p->type2.channel);
                    }
                }
                xSemaphoreTake(bufferMutex, portMAX_DELAY);
                std::swap(inputBuffer, outputBuffer);
                xSemaphoreGive(bufferMutex);
                xTaskNotifyGive(process_task_handle);
                ESP_LOGD(TAG, "OK");


            } else {
                ESP_LOGE(TAG, "Error on ADC buffer size: %lub - Expected: %ib", ret_num, ADC_BUFFER_SIZE);
            }
            //taskYIELD();
            //vTaskDelay(pdMS_TO_TICKS(15)); // 10ms delay
        } else {
            ESP_LOGE(TAG, "ADC continuous read failed: %s", esp_err_to_name(ret));
        }

        //chrono.endCycle();

        uint64_t end_time = esp_timer_get_time();
        ESP_LOGD(TAG, "Task execution time: %llu µs", end_time - curentTimestamp);
    }
}
