#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "adc.h"
//#include "wifi.h"
//#include "measure.h"


/*extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    //wifi_init_sta();
    
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 0);

    //start_webserver();
    
}*/


extern "C" void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    //bufferReadyQueue = xQueueCreate(2, sizeof(std::array<adc_reading_t, NB_SAMPLES>*));

    //xTaskCreatePinnedToCore(process_and_log_task, "Process and Log Task", 8192, NULL, 4, &process_task_handle, 1);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, 5, &adc_task_handle, 0);
}
