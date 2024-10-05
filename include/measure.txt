#ifndef __MEASURE_H
#define __MEASURE_H

#include <math.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <queue>
#include <mutex>

#include "def.h"
#include "signals.h"

#define NB_FFT_CHANNELS 2

#define ADC_BITS            12.
#define ADC_COEFF_A         (500. / pow(2., ADC_BITS))
#define ADC_COEFF_B         127.


class Measure
{
public:
    struct Data {
        time_t timestamp;
        float duration;
        Tension::Data tension;
        Current::Data currents[NB_CURRENTS];
    };

    Measure();
    ~Measure();
    void init();
    void adcCallback(volatile int* data);
    std::string getJsonOld();
    std::string getJson();
    void packetTask();
    bool popFromQueue(Data &data);
    std::string getBufferJson();


private:

    void save();
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
    cJSON* m_data;
    std::queue<Data> m_fifo;
    std::mutex m_queueMutex;
    cJSON* serializeData(Data &data);

    bool m_firstError;
    //uint16_t m_iPeriodTimeBuffer;
    //std::vector<float> m_periodTimeBuffer;
};

extern Measure measure;

void packeting_task(void *pvParameters);

#endif      // __MEASURE_H
