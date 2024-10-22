#ifndef __ADC_H
#define __ADC_H

#include "def.h"

#define ADC_BUFFER_SIZE (NB_CHANNELS * NB_SAMPLES * SOC_ADC_DIGI_RESULT_BYTES)

void adc_task(void *pvParameters);

static const std::array<adc_channel_t, NB_CHANNELS> ADC_CHANNELS = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_3, ADC_CHANNEL_4,
    ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8
};  // Keep ADC1_2 (pin3) free for JTAG
//pinout ref: https://m.media-amazon.com/images/I/71PHTB8JfiL._AC_SL1293_.jpg



#endif      // __ADC_H
