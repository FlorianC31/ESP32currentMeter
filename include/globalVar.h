#ifndef __GLOBAL_VAR_H__
#define __GLOBAL_VAR_H__

#include <freertos/FreeRTOS.h>
#include <vector>

#include "chrono.h"
#include "circularBuffer.h"

extern TaskHandle_t adc_task_handle;
extern TaskHandle_t process_task_handle;

extern Chrono adcChrono;
extern Chrono chronoChrono;
extern Chrono bufferMutexChrono;
extern Chrono bufferTotalChrono;

extern CircularBuffer adcBuffer;

// Define the queue handle
extern QueueHandle_t adcDataQueue;

extern std::vector<Chrono*> chronoList;

#endif      // __GLOBAL_VAR_H__
