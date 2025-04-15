#include "def.h"
#include "globalVar.h"

#include "adc.h"
#include "chrono.h"
#include "wifi.h"
#include "server.h"
#include "circularBuffer.h"
#include "measure.h"
#include "iirFilter.h"
#include "fft.h"

TaskHandle_t process_task_handle = NULL;
QueueHandle_t adcDataQueue = NULL;
TaskHandle_t memory_handle = NULL;
TaskHandle_t fft_handle = NULL;

int nbIgnoredPeriods = 20;

Chrono chronoChrono("Chrono", 0.2, 12);
Chrono adcChrono("Adc", 3, nbIgnoredPeriods);
Chrono fftChrono("FFT", 10, nbIgnoredPeriods / NB_BUFF_CYCLES);
Chrono convertChrono("Conversion", 2, nbIgnoredPeriods * NB_SAMPLES);
Chrono processChrono("Process", 2, nbIgnoredPeriods * NB_SAMPLES);
Chrono bufferMutexChrono("Buffer Mutex", 2);
Chrono bufferTotalChrono("Buffer Total", 2);
std::vector<Chrono*> chronoList = {&adcChrono, &fftChrono, &convertChrono, &processChrono};

CircularBuffer adcBuffer = CircularBuffer();
Measure measure = Measure();
ErrorManager errorManager = ErrorManager();

std::array<float, NB_SIGNALS> calibCoeffA = {CURRENT1_COEF_A, CURRENT2_COEF_A, CURRENT3_COEF_A, CURRENT4_COEF_A, CURRENT5_COEF_A, CURRENT6_COEF_A, CURRENT7_COEF_A, CURRENT8_COEF_A, TENSION_COEF_A};
std::array<IIRFilter, NB_CHANNELS> iirFilters;

std::array<float, NB_SIGNALS> convertRawData(std::array<uint16_t, NB_CHANNELS> adcRawData)
{
    std::array<float, NB_SIGNALS> convertedData;
    float vref = iirFilters[VREF_ID].process(adcRawData[VREF_ID]);
    for (uint8_t channelId = 0; channelId < NB_SIGNALS; channelId++) {
        convertedData[channelId] = calibCoeffA[channelId] * (iirFilters[channelId].process(adcRawData[channelId]) - vref);
    }
    return convertedData;
}

float calcZeroCrossingIndex(float x1, float y1, float x2, float y2)
{
    float y0 = 0;   // pow(2, 12) / 2.; // y0 = 2048 for 12-bit ADC
    float a = (y2 - y1) / (x2 - x1);
    float b = y1 - a * x1; 
    float x0 = (y0 - b) / a; // x0 = -b/a
    if (x0 < x1 || x0 > x2) {
        ESP_LOGW("Zero crossing", "Invalid zero crossing index: %f", x0);
        return -1.;
    }
    if (x0 < 0.) {
        ESP_LOGW("Zero crossing", "Negative zero crossing index: %f", x0);
        return -1.;
    }
    return x0;
}

/**
 * @brief Calculate the period of the tension signal
 * @param signals Pointer to the array of signals
 * @return The period of the tension signal in seconds, or -1 if not found
 * @details 
    // This function detects the period of the tension signal by looking for zero crossings
    // It uses the raising edges or the falling edges in function of the first one detected
 */
float getTensionPeriod(std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS>* signals)
{
    constexpr float ERROR_VALUE = -1.0f;
    float lastTension = signals->at(TENSION_ID)[0];
    float firstZcIndex = -1.;
    float secondZcIndex = -1.;

    enum EdgeType {NONE, RISING, FALLING} edgeType = NONE;

    for (uint16_t i = 1; i < BUFFER_SIZE; i++) {
        float currentTension = signals->at(TENSION_ID)[i];
        
        // Raising edge detection
        if (edgeType != FALLING && lastTension < 0 && currentTension >= 0) {
            edgeType = RISING;
            float zeroCrossingTimestamp = calcZeroCrossingIndex(i - 1, lastTension, i, currentTension);
            if (zeroCrossingTimestamp >= 0) {
                if (firstZcIndex == -1.) {
                    firstZcIndex = zeroCrossingTimestamp;
                }
                else {
                    secondZcIndex = zeroCrossingTimestamp;
                    break;
                }
            }
        }

        // Falling edge detection
        if (edgeType != RISING && lastTension > 0 && currentTension <= 0) {
            edgeType = FALLING;
            float zeroCrossingTimestamp = calcZeroCrossingIndex(i - 1, lastTension, i, currentTension);
            if (zeroCrossingTimestamp >= 0) {
                if (firstZcIndex == -1.) {
                    firstZcIndex = zeroCrossingTimestamp;
                }
                else {
                    secondZcIndex = zeroCrossingTimestamp;
                    break;
                }
            }
        }

        lastTension = currentTension;
    }

    if (firstZcIndex == -1. || secondZcIndex == -1.) {
        ESP_LOGW("Tension", "Period not found between similar edges");
        return ERROR_VALUE;
    }

    float period = (secondZcIndex - firstZcIndex) / MAIN_FREQ / NB_SAMPLES; // in seconds
    float freq = 1. / period;

    // Robustess check on the frequency
    if (freq > MAX_AC_FREQ || freq < MIN_AC_FREQ) {
        ESP_LOGW("Tension", "Period outside expected range: %fHz - ZcIndexes : %f-%f", freq, firstZcIndex, secondZcIndex);
        return ERROR_VALUE;
    }

    return period;
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

    //fft_config_t *real_fft_plan = fft_init(BUFFER_SIZE, FFT_REAL, FFT_FORWARD, NULL, NULL);
    
    while (1) {
        if (xQueueReceive(adcDataQueue, &adcRawData, 1) == pdPASS) {
            processChrono.startCycle();
            convertChrono.startCycle();
            adcConvertedData = convertRawData(adcRawData);
            convertChrono.endCycle();
            if (adcBuffer.addData(adcConvertedData)) {
                xTaskNotifyGive(fft_handle);
            }

            /*fftChrono.startCycle();
            real_fft_plan->input = adcConvertedData.data();
            fftChrono.endCycle();
            fft_execute(real_fft_plan);

            ESP_LOGW(TAG, "DC component : %f\n", real_fft_plan->output[0]);  // DC is at [0]
            for (int k = 1 ; k < real_fft_plan->size / 2 ; k++) {
                ESP_LOGW(TAG, "%d-th freq : %f+j%f\n", k, real_fft_plan->output[2*k], real_fft_plan->output[2*k+1]);
            }
            ESP_LOGW(TAG, "Middle component : %f\n", real_fft_plan->output[1]);  // N/2 is real and stored at [1]*/

            //measure.cal(adcData);   
            processChrono.endCycle();     
        }
    }
}


void fftTask(void *pvParameters) {
    //static const char* TAG = "FFT";

    fft_config_t *real_fft_plan = fft_init(BUFFER_SIZE, FFT_REAL, FFT_FORWARD, NULL, NULL);

    float meanPeriod = 0.;
    int periodCount = 0;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        float currentTensionPeriod = getTensionPeriod(adcBuffer.getData());
        if (currentTensionPeriod > 0.) {
            meanPeriod = (meanPeriod * periodCount + currentTensionPeriod) / (periodCount + 1);
            periodCount++;
            if (periodCount == NB_PERIODS_MEAN) {
                ESP_LOGW("TENSION", "Mean frequency: %fHz\n", 1. / meanPeriod);
                periodCount = 0;
            }
        } else {
            ESP_LOGW("TENSION", "Invalid period: %f\n", currentTensionPeriod);
        }

    
        fftChrono.startCycle();
        for (int signal = 0; signal < NB_SIGNALS; signal++) {
            real_fft_plan->input = adcBuffer.getData()->at(signal).data();
            fft_execute(real_fft_plan);
            /*if (signal == 3) {
                for (int k = 1 ; k <=7 ; k+=2) {
                    ESP_LOGW(TAG, "Signal %d - Harmonic %d: %f+j%f", signal, k, real_fft_plan->output[2*k], real_fft_plan->output[2*k+1]);
                }
            }*/
        }
        fftChrono.endCycle();
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
    xTaskCreatePinnedToCore(adc_task, "ADC Task", 8192, NULL, configMAX_PRIORITIES - 1, &adc_task_handle, 0);
    xTaskCreatePinnedToCore(fftTask, "FFT Task", 8192, NULL, 5, &fft_handle, 0);

    //xTaskCreatePinnedToCore(memory_task, "MEMORY Task", 8192, NULL, 5, &memory_handle, 1);


    

    ESP_LOGW(TAG, "Tasks created, application running");
}
