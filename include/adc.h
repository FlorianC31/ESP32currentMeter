#ifndef __ADC_H
#define __ADC_H

#include "def.h"
//#include "chrono.h"

#define ADC_BUFFER_SIZE (NB_CHANNELS * NB_SAMPLES * SOC_ADC_DIGI_RESULT_BYTES)

void adc_task(void *pvParameters);

//extern const std::array<adc_channel_t, NB_CHANNELS> ADC_CHANNELS;


//extern volatile int adcValues[NB_CHANNELS];

//extern std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* inputBuffer;
//extern std::array<std::array<uint32_t, NB_CHANNELS>, NB_SAMPLES>* outputBuffer;

//extern Chrono adcChrono;

#endif      // __ADC_H
