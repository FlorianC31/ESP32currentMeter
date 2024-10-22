#include "adc.h"
#include "chrono.h"

#include <string.h>



static const char *TAG = "ADC";

TaskHandle_t adc_task_handle = NULL;


static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config;
    adc_config.max_store_buf_size = ADC_BUFFER_SIZE;
    adc_config.conv_frame_size = ADC_BUFFER_SIZE;
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg;
    dig_cfg.sample_freq_hz = static_cast<uint32_t>(SAMPLE_FREQ * NB_CHANNELS);
    dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX];
    dig_cfg.pattern_num = NB_CHANNELS;
    for (int i = 0; i < NB_CHANNELS; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = ADC_CHANNELS[i];
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%u", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%u", i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%u", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}


void adc_task(void *pvParameters) {
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[ADC_BUFFER_SIZE] = {0};
    memset(result, 0xcc, ADC_BUFFER_SIZE);

    adc_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(&handle);

    adc_continuous_evt_cbs_t cbs;
    cbs.on_conv_done = s_conv_done_cb;
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    std::array<uint16_t, NB_CHANNELS> adcData;

    while (1) {

        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        adcChrono.startCycle();
        

        ret = adc_continuous_read(handle, result, ADC_BUFFER_SIZE, &ret_num, 0);
        if (ret == ESP_OK) {
            uint8_t channelId = 0;
            //ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                uint16_t chanNum = static_cast<uint16_t>(p->type2.channel);
                uint16_t data = static_cast<uint16_t>(p->type2.data);
                /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
                if (chanNum < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1) && ADC_CHANNELS[channelId] == chanNum) {
                    //ESP_LOGI(TAG, "Unit: %s, Channel: %u, Value: %u", "ADC1", chanNum, data);
                    adcData[channelId] = data;
                } else {
                    ESP_LOGW(TAG, "Invalid data [%s_%u_%u]", "ADC1", chanNum, data);
                }

                channelId++;
                if (channelId == NB_CHANNELS) {
                    channelId = 0;
                    if (xQueueSend(adcDataQueue, &adcData, 1) != pdPASS) {
                        ESP_LOGE(TAG, "Error sending ADC data to the queue");
                    }
                    else {
                        //ESP_LOGE(TAG, "ADC data successfully sent to the queue - %i", nbSample);
                    }
                }

                
            }
            /**
             * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
             * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
             * usually you don't need this delay (as this task will block for a while).
             */
            //vTaskDelay(1);
        } else if (ret == ESP_ERR_TIMEOUT) {
            //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
            break;
        }

        chronoChrono.startCycle();
        adcChrono.endCycle();
        chronoChrono.endCycle();
    }
    
    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}