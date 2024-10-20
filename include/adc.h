#ifndef __ADC_H
#define __ADC_H

#include "def.h"

#define ADC_BUFFER_SIZE (NB_CHANNELS * SAMPLES_IN_BUFF * SOC_ADC_DIGI_RESULT_BYTES)

void adc_task(void *pvParameters);
//void adc_init();

static const std::array<adc_channel_t, NB_CHANNELS> ADC_CHANNELS = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3,
    ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7
};



#endif      // __ADC_H
