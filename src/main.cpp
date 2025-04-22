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

TaskHandle_t process_task_handle = NULL;
TaskHandle_t analysis_task_handle = NULL;
TaskHandle_t memory_handle = NULL;
TaskHandle_t fft_handle = NULL;
QueueHandle_t adcDataQueue = NULL;
std::array<ElecSignal*, NB_CHANNELS> signalsData = {nullptr};

int nbIgnoredPeriods = 20;

Chrono chronoChrono("Chrono", 0.2, 12);
Chrono adcChrono("Adc", 3, nbIgnoredPeriods * BUFFER_SIZE);
Chrono fftChrono("FFT", 10, nbIgnoredPeriods);
Chrono convertChrono("Conversion", 2, nbIgnoredPeriods * NB_SAMPLES);
Chrono processChrono("Process", 2, nbIgnoredPeriods * NB_SAMPLES);
Chrono bufferMutexChrono("Buffer Mutex", 2);
Chrono bufferTotalChrono("Buffer Total", 2);
std::vector<Chrono*> chronoList = {&adcChrono, &fftChrono, &convertChrono, &processChrono};

std::array<uint16_t, NB_CHANNELS> adcRawData;
std::array<float, NB_CURRENTS> currentCalibCoeff = {CURRENT1_COEF, CURRENT2_COEF, CURRENT3_COEF, CURRENT4_COEF, CURRENT5_COEF, CURRENT6_COEF, CURRENT7_COEF, CURRENT8_COEF};



/**
 * * @brief Initialize the signals
 */
void initSignals()
{
    fft_config_t *fftManager = (fft_config_t *)malloc(sizeof(fft_config_t));

    signalsData[VREF_ID] = new ElecSignal("Vref", false, fftManager);
    signalsData[TENSION_ID] = new ElecSignal("Tension", true, fftManager, TENSION_COEF, signalsData[VREF_ID]);

    for (uint8_t i = 0; i < NB_CURRENTS; i++) {
        std::string signalName = "Current" + std::to_string(i + 1);
        signalsData[i + 1] = new ElecSignal(signalName, false, fftManager, currentCalibCoeff[i], signalsData[VREF_ID], signalsData[TENSION_ID]);
    }
}





/**
 * @brief Process task function
 * 
 * This task processes the ADC data.
 */
void process(void *pvParameters) {

    static const char* TAG = "PROCESS_TASK";

    ESP_LOGI(TAG, "Process task starting");
    
    while (1) {
        if (xQueueReceive(adcDataQueue, &adcRawData, 1) == pdPASS) {
            for (uint8_t i = 0; i < NB_CHANNELS; i++) {
                //ESP_LOGI(TAG, "addRawData(adcRawData[%i])", i);
                signalsData[i]->addRawData(adcRawData[i]);
            }   
            processChrono.endCycle();
            if (signalsData[VREF_ID]->isReadyForProcessing()) {
                xTaskNotify(analysis_task_handle, 0x01, eSetBits);
            }
        }
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
                signalsData[TENSION_ID]->runAnalysis();
                for (uint8_t i = 1; i <= NB_CURRENTS; i++) {
                    signalsData[i]->runAnalysis();
                }
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

    adcDataQueue = xQueueCreate(QUEUE_SIZE, sizeof(std::array<uint16_t, NB_CHANNELS>));
    if (adcDataQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC data queue");
        vTaskDelete(NULL);
    }

    start_webserver();
    initSignals();

    xTaskCreatePinnedToCore(process, "Process Task", 8192, NULL, 4, &process_task_handle, 0);
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, configMAX_PRIORITIES - 1, &adc_task_handle, 0);
    xTaskCreatePinnedToCore(dataAnalysis, "Data Analysis Task", 8192, NULL, configMAX_PRIORITIES - 1, &analysis_task_handle, 1);
    //xTaskCreatePinnedToCore(memory_task, "MEMORY Task", 8192, NULL, 5, &memory_handle, 1);


    

    ESP_LOGW(TAG, "Tasks created, application running");
}
