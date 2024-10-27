#ifndef __GLOBAL_VAR_H__
#define __GLOBAL_VAR_H__

#include <freertos/FreeRTOS.h>
#include <vector>

#include "chrono.h"
#include "circularBuffer.h"
#include "errorManager.h"

extern TaskHandle_t adc_task_handle;
extern TaskHandle_t process_task_handle;

extern std::vector<Chrono*> chronoList;
extern Chrono adcChrono;
extern Chrono chronoChrono;
extern Chrono bufferMutexChrono;
extern Chrono bufferTotalChrono;

extern CircularBuffer adcBuffer;

extern QueueHandle_t adcDataQueue;

extern ErrorManager errorManager;

#endif      // __GLOBAL_VAR_H__
