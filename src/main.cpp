#include "def.h"
#include "adc.h"
#include "chrono.h"

TaskHandle_t process_task_handle = NULL;
QueueHandle_t adcDataQueue = NULL;

Chrono chrono("Process", 20.5, 200, true);
Chrono* adcChrono = new Chrono("Adc", 20.5, 20, true);

/**
 * @brief Process and log task function
 * 
 * This task processes the ADC data and logs the results.
 */
void process_and_log_task(void *pvParameters) {

    static const char* TAG = "PROCESS_TASK";

    ESP_LOGI(TAG, "Process and log task starting");
    std::string tensionValues = "Tension Values";
    std::string vrefValues = "Vref Values";

    uint16_t nbSamples = 0;
    std::array<uint32_t, NB_CHANNELS> adcData;

    while (1) {
        if (xQueueReceive(adcDataQueue, &adcData, 1) == pdPASS) {
            chrono.endCycle();
            chrono.startCycle();

            
            tensionValues += ";" + std::to_string(adcData[TENSION_ID]);
            vrefValues += ";" + std::to_string(adcData[VREF_ID]);

            nbSamples++;

            if (nbSamples == 2 * NB_SAMPLES) {
                ESP_LOGW(TAG, "%s", tensionValues.c_str());
                ESP_LOGW(TAG, "%s", vrefValues.c_str());
                tensionValues = "Tension Values";
                vrefValues = "Vref Values";
                nbSamples = 0;
            }
        }
    }
}

extern "C" void app_main(void) {
    static const char* TAG = "MAIN";

    ESP_LOGW(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    adcDataQueue = xQueueCreate(NB_SAMPLES * 2, sizeof(std::array<uint32_t, NB_CHANNELS>));
    if (adcDataQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC data queue");
        vTaskDelete(NULL);
    }

    xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 1);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 0);

    ESP_LOGW(TAG, "Tasks created, application running");
}
