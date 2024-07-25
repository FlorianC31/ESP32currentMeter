#ifndef __ADC_H
#define __ADC_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <esp_adc/adc_continuous.h>

#include "chrono.h"

#define NUM_CHANNELS 7
#define SAMPLE_RATE 1000
#define DMA_BUFFER_SIZE (1024 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV)

void adc_task(void *pvParameters);

extern SemaphoreHandle_t mutex;
extern volatile int adcValues[NUM_CHANNELS];

#endif      // __ADC_H
