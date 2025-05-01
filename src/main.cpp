#include "def.h"
#include "globalVar.h"

#include "adc.h"
#include "chrono.h"
#include "wifi.h"
#include "server.h"
#include "elecSignal.h"

#include "driver/ledc.h"
#define PWM_GPIO 40        // GPIO 40 pour la sortie
#define PWM_FREQ_HZ 50     // Fréquence 50Hz
#define PWM_RESOLUTION LEDC_TIMER_12_BIT  // Résolution 12-bit (0-4095)
#define PWM_CHANNEL LEDC_CHANNEL_0        // Canal LEDC 0
#define PWM_DUTY 2048       // 50% duty cycle (2048 sur 4095 pour 12-bit)

TaskHandle_t buffering_task_handle = NULL;
TaskHandle_t analysis_task_handle = NULL;
TaskHandle_t memory_handle = NULL;
std::array<ElecSignal*, NB_SIGNALS> signalsData = {nullptr};

int nbIgnoredPeriods = 20;

Chrono chronoChrono("Chrono");
Chrono adcChrono("Adc");
Chrono fftChrono("FFT");
Chrono bufferingChrono("Buffering", MAIN_FREQ * NB_SAMPLES);
Chrono processChrono("Process");
Chrono bufferMutexChrono("Buffer Mutex", 2);
Chrono bufferTotalChrono("Buffer Total", 2);
std::vector<Chrono*> chronoList = {&adcChrono, &fftChrono, &bufferingChrono, &processChrono};

std::array<uint16_t, NB_CHANNELS> adcRawData;
std::array<float, NB_CURRENTS> currentCalibCoeff = {CURRENT1_COEF, CURRENT2_COEF, CURRENT3_COEF, CURRENT4_COEF, CURRENT5_COEF, CURRENT6_COEF, CURRENT7_COEF, CURRENT8_COEF};



/**
 * * @brief Initialize the signals
 */
void initSignals()
{
    fft_config_t *fftManager = (fft_config_t *)malloc(sizeof(fft_config_t));

    signalsData[TENSION_ID - 1] = new ElecSignal("Tension", true, fftManager, TENSION_COEF);

    for (uint8_t i = 0; i < NB_CURRENTS; i++) {
        uint8_t currentId = TENSION_ID + i;
        std::string signalName = "Current" + std::to_string(currentId);
        signalsData[currentId] = new ElecSignal(signalName, false, fftManager, currentCalibCoeff[i], signalsData[TENSION_ID]);
    }
}








/**
 * @brief Process task function
 * 
 * This task processes the ADC data.
 */
void dataAnalysis(void *pvParameters) {

    static const char* TAG = "ANALYSIS_TASK";
    ESP_LOGI(TAG, "Analysis task starting");

    uint32_t ulNotificationValue;
    
    while (1) {
        if(xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY) == pdTRUE) {
            if((ulNotificationValue & 0x01) != 0) {
                processChrono.startCycle();
                signalsData[TENSION_ID]->runAnalysis();
                for (uint8_t i = 1; i <= NB_CURRENTS; i++) {
                    signalsData[i]->runAnalysis();
                }
                processChrono.endCycle();
            }
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


    start_webserver();
    initSignals();

    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, configMAX_PRIORITIES - 1, &adc_task_handle, 0);
    xTaskCreatePinnedToCore(dataAnalysis, "Data Analysis Task", 8192, NULL, configMAX_PRIORITIES - 1, &analysis_task_handle, 1);
    //xTaskCreatePinnedToCore(memory_task, "MEMORY Task", 8192, NULL, 5, &memory_handle, 1);


    

    ESP_LOGW(TAG, "Tasks created, application running");
}
