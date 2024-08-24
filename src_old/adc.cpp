#include "adc.h"
#include "measure.h"
#include <string>
#include <esp_log.h>

// Mutex for synchronizing access to shared resources
SemaphoreHandle_t mutex = nullptr;

// Array to store ADC values for each channel
volatile int adcValues[NB_CHANNELS] = {0};

std::string buffer("");
uint16_t iBuffer(0);

// Array of ADC channels to be sampled
static const adc_channel_t ADC_CHANNELS[NB_CHANNELS] = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6
};

// Flag to indicate if an action is required
static volatile bool actionFlag = false;

// Pointer to the raw ADC data buffer
static uint8_t *adc_raw;

// Handle for the ADC continuous mode
static adc_continuous_handle_t adc_handle = NULL;

// Chrono to measure ADC convertion time
Chrono adcChrono("ADC", 100, SAMPLE_RATE, DEBUG);

/**
 * @brief ADC conversion done callback function.
 *
 * This function is called when the ADC conversion is done. It notifies the task
 * that is waiting for the ADC conversion to complete.
 *
 * @param handle ADC continuous handle.
 * @param edata Pointer to the ADC continuous event data.
 * @param user_data Pointer to the user data (task handle in this case).
 * @return True if a higher priority task was woken, otherwise false.
 */
static bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    BaseType_t high_task_awoken = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(user_data), &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

/**
 * @brief ADC timer callback function.
 *
 * This function is called periodically by the timer. It reads the ADC values,
 * processes them, and updates the shared resources.
 *
 * @param arg Pointer to the argument passed to the timer callback.
 */
static void adc_timer_callback(void* arg) {
    adcChrono.startCycle();
    BaseType_t high_task_awoken = pdFALSE;

    for (int i = 0; i < NB_CHANNELS; i++) {
        xSemaphoreTakeFromISR(mutex, &high_task_awoken);
        uint32_t adc_reading = ((uint32_t*)adc_raw)[i];
        adcValues[i] = adc_reading & 0xFFF;
        xSemaphoreGiveFromISR(mutex, &high_task_awoken);
    }

    //measure.adcCallback(adcValues);
    if (iBuffer < (NB_SAMPLES * 4)) {
        buffer += std::to_string(adcValues[VREF_ID]) + ";";
        iBuffer++;
        ESP_LOGI("Phase 1", "iBuffer : %i", iBuffer);

        if (iBuffer == (NB_SAMPLES * 4)) {
        ESP_LOGI("Phase 2", "iBuffer : %i", iBuffer);
            ESP_LOGI("Buffer", "Tension : %s", buffer.c_str());
        }
    }
 
    if (high_task_awoken) {
        portYIELD_FROM_ISR();
    }

    adcChrono.endCycle();
}

/**
 * @brief ADC task function.
 *
 * This function initializes the ADC, configures it, and starts the ADC continuous
 * mode. It then waits for notifications from the ADC conversion done callback
 * and processes the ADC data.
 *
 * @param pvParameters Pointer to the task parameters (not used in this case).
 */
void adc_task(void *pvParameters) {
    esp_err_t ret;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = DMA_BUFFER_SIZE,
        .conv_frame_size = SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NB_CHANNELS,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg;
    dig_cfg.sample_freq_hz = SAMPLE_RATE * NB_CHANNELS;
    dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;

    adc_digi_pattern_config_t adc_pattern[NB_CHANNELS];
    for (int i = 0; i < NB_CHANNELS; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_11;
        adc_pattern[i].channel = ADC_CHANNELS[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    dig_cfg.pattern_num = NB_CHANNELS;
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
