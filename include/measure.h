#ifndef __MEASURE_H
#define __MEASURE_H

#include <math.h>
#include <stdint.h>
#include <vector>

#include "def.h"
#include "signals.h"

#define NB_FFT_CHANNELS 2

#define ADC_BITS            12.
#define ADC_COEFF_A         (500. / pow(2., ADC_BITS))
#define ADC_COEFF_B         127.
#define DATA_SENT_PERIOD    10.   // 5. * 60.    // 5 minutes



class Measure
{
public:
    Measure();
    ~Measure() {}
    void init();
    void adcCallback(uint32_t* data);


private:
    typedef enum {
        INIT = 0,
        WAITING_ZC,
        NORMAL_PHASE
    } InitState;

    std::vector<Current> m_currents;
    Tension m_tension;
    InitState m_initState;
    float m_timerPeriod;
    float m_totalMeasureTime;
    float m_periodTime;
    //uint16_t m_iPeriodTimeBuffer;
    //std::vector<float> m_periodTimeBuffer;
};

#endif      // __MEASURE_H
