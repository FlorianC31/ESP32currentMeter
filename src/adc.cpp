#include <string.h>

#include "globalVar.h"
#include "adc.h"
#include "chrono.h"

// Define the adc_timer_callback function
void adc_timer_callback(void* arg) {
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);
}

static const char *TAG = "ADC";

TaskHandle_t adc_task_handle = NULL;


/*static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(adc_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}*/

static void continuous_adc_init(adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;
    adc_continuous_handle_cfg_t adc_config;
    adc_config.max_store_buf_size = ADC_BUFFER_SIZE * 2;
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
    
    // Configurer l'ADC
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));
    
    // Constantes pour les filtres
    constexpr adc_digi_iir_filter_coeff_t TENSION_FILTER_COEFF = ADC_DIGI_IIR_FILTER_COEFF_64;
    //constexpr adc_digi_iir_filter_coeff_t VREF_FILTER_COEFF = ADC_DIGI_IIR_FILTER_COEFF_64;

    // Configuration et activation du filtre pour la tension
    adc_iir_filter_handle_t tension_filter_handle;
    adc_continuous_iir_filter_config_t tension_filter_config = {
        .unit = ADC_UNIT_1,
        .channel = ADC_CHANNELS[TENSION_ID],
        .coeff = TENSION_FILTER_COEFF
    };
    
    // Configuration et activation du filtre pour VREF
    /*adc_iir_filter_handle_t vref_filter_handle;
    adc_continuous_iir_filter_config_t vref_filter_config = {
        .unit = ADC_UNIT_1,
        .channel = ADC_CHANNELS[VREF_ID],
        .coeff = VREF_FILTER_COEFF
    };*/
    
    // Création des filtres
    esp_err_t ret = adc_new_continuous_iir_filter(handle, &tension_filter_config, &tension_filter_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create IIR filter for tension channel: %s", esp_err_to_name(ret));
        return;
    }
    else {
        ESP_LOGI(TAG, "IIR filter created for tension channel: %d", ADC_CHANNELS[TENSION_ID]);
    }

    /*ret = adc_new_continuous_iir_filter(handle, &vref_filter_config, &vref_filter_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create IIR filter for VREF channel: %s", esp_err_to_name(ret));
        adc_del_continuous_iir_filter(tension_filter_handle);
        return;
    }
    else {
        ESP_LOGI(TAG, "IIR filter created for VREF channel: %d", ADC_CHANNELS[VREF_ID]);
    }*/

    // Activation des filtres
    ret = adc_continuous_iir_filter_enable(tension_filter_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable tension filter: %s", esp_err_to_name(ret));
        return;
    }
    else {
        ESP_LOGI(TAG, "IIR filter enabled for tension channel %d with coeff %d", ADC_CHANNELS[TENSION_ID], TENSION_FILTER_COEFF);
    }

    /*ret = adc_continuous_iir_filter_enable(vref_filter_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable VREF filter: %s", esp_err_to_name(ret));
        return;
    }
    else {
        ESP_LOGI(TAG, "IIR filter enabled for VREF channel %d with coeff %d", ADC_CHANNELS[VREF_ID], VREF_FILTER_COEFF);
    }*/

    
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
    //cbs.on_conv_done = s_conv_done_cb;
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    esp_timer_create_args_t timer_args;
    timer_args.callback = &adc_timer_callback;
    timer_args.name = "adc_timer";
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, MAIN_PERIOD)); // 20ms = 50Hz

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

        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "ADC Timeout error");
        }

        chronoChrono.startCycle();
        adcChrono.endCycle();
        chronoChrono.endCycle();
    }
    
    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
