#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include "nvs_flash.h"

#include "adc.h"
#include "wifi.h"
#include "measure.h"


extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    mutex = xSemaphoreCreateMutex();
    
    //wifi_init_sta();
    
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 4096, NULL, 5, NULL, 0);

    //start_webserver();
    
}