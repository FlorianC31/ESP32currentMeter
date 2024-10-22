#include "def.h"
#include "adc.h"
#include "chrono.h"
#include "wifi.h"

TaskHandle_t process_task_handle = NULL;
QueueHandle_t adcDataQueue = NULL;

Chrono chrono("Process", 48, 10, 10);
Chrono chronoChrono("Chrono", 48, 0.2, 10);
Chrono adcChrono("Adc", 48, 2, 10);

SemaphoreHandle_t bufferMutex = NULL;
uint8_t bufferPos = 0;
std::array<std::array<uint8_t, NB_CHANNELS>, NB_SAMPLES> adcBuffer;

/**
 * @brief Process and log task function
 * 
 * This task processes the ADC data and logs the results.
 */
void process_and_log_task(void *pvParameters) {

    static const char* TAG = "PROCESS_TASK";

    ESP_LOGI(TAG, "Process and log task starting");
    std::string tensionValues;
    std::string vrefValues;
    tensionValues.reserve(14 + NB_SAMPLES * 1 * 5);
    vrefValues.reserve(14 + NB_SAMPLES * 1 * 5); 
    tensionValues = "Tension Values";
    vrefValues = "Vref Values";

    //uint16_t nbSamples = 0;
    std::array<uint8_t, NB_CHANNELS> adcData;

    while (1) {
        if (xQueueReceive(adcDataQueue, &adcData, 1) == pdPASS) {
            chrono.endCycle();
            chrono.startCycle();

            xSemaphoreTake(bufferMutex, portMAX_DELAY);
            adcBuffer[bufferPos] = adcData;

            bufferPos++;
            if (bufferPos == NB_SAMPLES) {
                bufferPos = 0;
            }
            xSemaphoreGive(bufferMutex);


            /*tensionValues += ";" + std::to_string(adcData[TENSION_ID]);
            vrefValues += ";" + std::to_string(adcData[VREF_ID]);

            nbSamples++;

            if (nbSamples == 1 * NB_SAMPLES) {
                printChrono.startCycle();
                //ESP_LOGW(TAG, "%s", tensionValues.c_str());
                //ESP_LOGW(TAG, "%s", vrefValues.c_str());
                printChrono.endCycle();
                tensionValues = "Tension Values";
                vrefValues = "Vref Values";
                nbSamples = 0;
            }*/
        
        }
    }
}

void printMemory() {
    static const char* TAG = "Memory";

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);

    ESP_LOGW(TAG, "Total heap size (kB): %f", float(info.total_free_bytes + info.total_allocated_bytes) / 1000.);
    ESP_LOGW(TAG, "Free heap size (kB): %f", float(info.total_free_bytes) / 1000.);
    ESP_LOGW(TAG, "Allocated heap size (kB): %f", float(info.total_allocated_bytes) / 1000.);
    ESP_LOGW(TAG, "Minimum free heap size (kB): %f", float(info.minimum_free_bytes) / 1000.);
}


extern "C" void app_main(void) {
    static const char* TAG = "MAIN";

    ESP_LOGW(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    //adc_init();
    wifi_init_sta();
    bufferMutex = xSemaphoreCreateMutex();

    adcDataQueue = xQueueCreate(NB_SAMPLES, sizeof(std::array<uint8_t, NB_CHANNELS>));
    if (adcDataQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC data queue");
        vTaskDelete(NULL);
    }

    start_webserver();
    //printMemory();
    
    //vTaskDelay(pdMS_TO_TICKS(3*1000));
    //printMemory();

    xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 0);
    printMemory();
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 1);
    printMemory();

    

    ESP_LOGW(TAG, "Tasks created, application running");
}
