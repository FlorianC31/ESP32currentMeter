#include "def.h"
#include "adc.h"

TaskHandle_t process_task_handle = NULL;
uint32_t lastTimestampProcess = 0;


/**
 * @brief Process and log task function
 * 
 * This task processes the ADC data and logs the results.
 */
void process_and_log_task(void *pvParameters) {

    static const char* TAG = "PROCESS_TASK";

    ESP_LOGI(TAG, "Process and log task starting");

    uint32_t processed_count = 0;

    while (1) {        
        std::string tensionValues = "Tension Values";
        std::string vrefValues = "Vref Values";

        // Wait for notification from adc_task
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);        
        xSemaphoreTake(bufferMutex, portMAX_DELAY);


        uint32_t curentTimestamp = esp_timer_get_time();
        if (lastTimestampProcess != 0) {
            uint32_t detlaT = (curentTimestamp - lastTimestampProcess);
            ESP_LOGW(TAG, "process_and_log_task - Actual Period : %luµs", detlaT);
        }
        lastTimestampProcess = curentTimestamp;

        /*for (const auto& sample : *outputBuffer) {
            tensionValues += ";" + std::to_string(sample[TENSION_ID]);
            vrefValues += ";" + std::to_string(sample[VREF_ID]);
        }*/
        xSemaphoreGive(bufferMutex);

        processed_count += NB_SAMPLES;
        ESP_LOGI(TAG, "Processed %lu samples", processed_count);

        //ESP_LOGI(TAG, "%s", tensionValues.c_str());
        //ESP_LOGI(TAG, "%s", vrefValues.c_str());
    }
}

extern "C" void app_main(void) {
    static const char* TAG = "MAIN";

    ESP_LOGI(TAG, "Application starting");
    ESP_ERROR_CHECK(nvs_flash_init());

    bufferMutex = xSemaphoreCreateMutex();
    //bufferReadyQueue = xQueueCreate(2, sizeof(std::array<adc_reading_t, NB_SAMPLES>*));

    xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 1);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 0);

    ESP_LOGI(TAG, "Tasks created, application running");
}
