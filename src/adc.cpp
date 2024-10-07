#include "adc.h"
#include "chrono.h"


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

//static QueueHandle_t bufferReadyQueue = NULL;

/*std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> buffer1;
std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> buffer2;

std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* inputBuffer(&buffer1);
std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* outputBuffer(&buffer2);*/


static adc_continuous_handle_t adc_handle = NULL;
TaskHandle_t adc_task_handle = NULL;

uint32_t lastTimestampAdc = 0;

uint32_t nbSamples = 0;


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
    nbSamples++;
    if (nbSamples == NB_SAMPLES) {
        nbSamples = 0;
        BaseType_t mustYield = pdFALSE;
        //Notify that ADC continuous driver has done enough number of conversions
        vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);

        return (mustYield == pdTRUE);
    }
    return false;
}


//void adc_init() {}


/**
 * @brief ADC task function
 * 
 * This task configures the ADC, starts continuous sampling, and processes the incoming data.
 */
void adc_task(void *pvParameters) {
    ESP_LOGI(TAG, "ADC task starting");

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

    std::array<uint32_t, NB_CHANNELS> adcData;

    esp_err_t ret;
    uint32_t ret_num = 0;
    std::array<uint8_t, ADC_BUFFER_SIZE> adc_raw;

    int nbSample = 0;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        adcChrono->startCycle();

        ret = adc_continuous_read(adc_handle, adc_raw.data(), adc_raw.size(), &ret_num, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC continuous read failed: %s", esp_err_to_name(ret));
            continue;
        }

        if (ret_num != ADC_BUFFER_SIZE) {
            ESP_LOGE(TAG, "Error on ADC buffer size: %lub - Expected: %ib", ret_num, ADC_BUFFER_SIZE);
            continue;
        }

        for (uint32_t sample = 0; sample < NB_SAMPLES; sample++) {
            for (uint32_t channel = 0; channel < NB_CHANNELS; channel++) {
                uint32_t index = (sample * NB_CHANNELS + channel) * SOC_ADC_DIGI_RESULT_BYTES;
                adc_digi_output_data_t *p = reinterpret_cast<adc_digi_output_data_t*>(&adc_raw[index]);
                if (p->type2.channel != channel) {
                    ESP_LOGE(TAG, "Error on channel: %u != %lu", p->type2.channel, channel);
                    continue;
                }
                adcData[p->type2.channel] = p->type2.data;
            }

            nbSample++;
            if (xQueueSend(adcDataQueue, &adcData, 1) != pdPASS) {
                ESP_LOGE(TAG, "Error sending ADC data to the queue - %i", nbSample);
            }
            else {
                //ESP_LOGE(TAG, "ADC data successfully sent to the queue - %i", nbSample);
            }

        }

        adcChrono->endCycle();
    }
}
