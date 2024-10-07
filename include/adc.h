#ifndef __ADC_H
#define __ADC_H

#include "def.h"

#define ADC_BUFFER_SIZE (NB_CHANNELS * SAMPLES_IN_BUFF * SOC_ADC_DIGI_RESULT_BYTES)

void adc_task(void *pvParameters);
//void adc_init();


#endif      // __ADC_H
