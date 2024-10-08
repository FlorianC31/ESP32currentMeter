#include "def.h"
#include "adc.h"
#include "chrono.h"
#include "wifi.h"

TaskHandle_t process_task_handle = NULL;
QueueHandle_t adcDataQueue = NULL;

Chrono chrono("Process", 20.5, 200);
Chrono printChrono("Print", 20.5, 1);
Chrono adcChrono("Adc", 20.5, 20);

SemaphoreHandle_t bufferMutex = NULL;
uint8_t bufferPos = 0;
std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES> adcBuffer;

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
    std::array<uint32_t, NB_CHANNELS> adcData;

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

extern "C" void app_main(void) {
    static const char* TAG = "MAIN";

    ESP_LOGW(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    //adc_init();
    wifi_init_sta();
    bufferMutex = xSemaphoreCreateMutex();

    adcDataQueue = xQueueCreate(SAMPLES_IN_BUFF, sizeof(std::array<uint32_t, NB_CHANNELS>));
    if (adcDataQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC data queue");
        vTaskDelete(NULL);
    }

    start_webserver();
    

    xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 1);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 32768, NULL, 5, &adc_task_handle, 0);

    

    ESP_LOGW(TAG, "Tasks created, application running");
}
