#ifndef __GLOBAL_VAR_H__
#define __GLOBAL_VAR_H__

#include <freertos/FreeRTOS.h>
#include <vector>
#include <string>

#include "chrono.h"
#include "errorManager.h"
#include "elecSignal.h"


extern TaskHandle_t adc_task_handle;
extern TaskHandle_t process_task_handle;

extern std::vector<Chrono*> chronoList;
extern Chrono adcChrono;
extern Chrono chronoChrono;
extern Chrono bufferMutexChrono;
extern Chrono bufferTotalChrono;

extern std::array<ElecSignal*, NB_CHANNELS> signalsData;

extern QueueHandle_t adcDataQueue;

extern ErrorManager errorManager;

extern std::string initTime;

#endif      // __GLOBAL_VAR_H__
