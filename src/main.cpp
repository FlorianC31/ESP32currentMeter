#include "def.h"
#include "globalVar.h"

#include "adc.h"
#include "chrono.h"
#include "wifi.h"
#include "server.h"
#include "circularBuffer.h"
#include "measure.h"
#include "fft.h"

TaskHandle_t process_task_handle = NULL;
QueueHandle_t adcDataQueue = NULL;
TaskHandle_t memory_handle = NULL;

Chrono chrono("Process", 10, 10);
Chrono chronoChrono("Chrono", 0.2, 10);
Chrono adcChrono("Adc", 2, 10);
Chrono fftChrono("FFT", 50);
Chrono bufferMutexChrono("Buffer Mutex", 2);
Chrono bufferTotalChrono("Buffer Total", 2);
std::vector<Chrono*> chronoList = {&adcChrono, &fftChrono, &chronoChrono, &bufferMutexChrono, &bufferTotalChrono};

CircularBuffer adcBuffer = CircularBuffer();
Measure measure = Measure();
ErrorManager errorManager = ErrorManager();

std::array<float, NB_SIGNALS> calibCoeffA = {CURRENT1_COEF_A, CURRENT2_COEF_A, CURRENT3_COEF_A, CURRENT4_COEF_A, CURRENT5_COEF_A, CURRENT6_COEF_A, TENSION_COEF_A};

std::array<float, NB_SIGNALS> convertRawData(std::array<uint16_t, NB_CHANNELS> adcRawData)
{
    std::array<float, NB_SIGNALS> convertedData;
    for (uint8_t channelId = 0; channelId < NB_SIGNALS; channelId++) {
        convertedData[channelId] = calibCoeffA[channelId] * (adcRawData[channelId] - adcRawData[VREF_ID]);
    }
    return convertedData;
}


/**
 * @brief Process and log task function
 * 
 * This task processes the ADC data and logs the results.
 */
void process_and_log_task(void *pvParameters) {

    static const char* TAG = "PROCESS_TASK";

    ESP_LOGI(TAG, "Process and log task starting");

    std::array<uint16_t, NB_CHANNELS> adcRawData;
    std::array<float, NB_SIGNALS> adcConvertedData;

    fft_config_t *real_fft_plan = fft_init(NB_SAMPLES, FFT_REAL, FFT_FORWARD, NULL, NULL);

    while (1) {
        if (xQueueReceive(adcDataQueue, &adcRawData, 1) == pdPASS) {
            adcConvertedData = convertRawData(adcRawData);
            adcBuffer.addData(adcConvertedData);

            fftChrono.startCycle();
            real_fft_plan->input = adcConvertedData.data();
            fftChrono.endCycle();
            fft_execute(real_fft_plan);

            ESP_LOGW(TAG, "DC component : %f\n", real_fft_plan->output[0]);  // DC is at [0]
            for (int k = 1 ; k < real_fft_plan->size / 2 ; k++) {
                ESP_LOGW(TAG, "%d-th freq : %f+j%f\n", k, real_fft_plan->output[2*k], real_fft_plan->output[2*k+1]);
            }
            ESP_LOGW(TAG, "Middle component : %f\n", real_fft_plan->output[1]);  // N/2 is real and stored at [1]

            //measure.cal(adcData);        
        }
    }
}

void memory_task(void *pvParameters) {
    static const char* TAG = "Memory";

    multi_heap_info_t info;

    while(1) {
        heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);

        //ESP_LOGW(TAG, "Total heap size (kB): %f", float(info.total_free_bytes + info.total_allocated_bytes) / 1000.);
        //ESP_LOGW(TAG, "Free heap size (kB): %f", float(info.total_free_bytes) / 1000.);
        ESP_LOGW(TAG, "Allocated heap size (kB): %f", float(info.total_allocated_bytes) / 1000.);
        //ESP_LOGW(TAG, "Minimum free heap size (kB): %f", float(info.minimum_free_bytes) / 1000.);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}




extern "C" void app_main(void) {
    static const char* TAG = "MAIN";

    ESP_LOGW(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    //adc_init();
    wifi_init_sta();

    adcDataQueue = xQueueCreate(NB_SAMPLES * NB_QUEUE_CYCLES, sizeof(std::array<uint16_t, NB_CHANNELS>));
    if (adcDataQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC data queue");
        vTaskDelete(NULL);
    }

    start_webserver();

    xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 0);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 0);

    //xTaskCreatePinnedToCore(memory_task, "MEMORY Task", 8192, NULL, 5, &memory_handle, 1);


    

    ESP_LOGW(TAG, "Tasks created, application running");
}
