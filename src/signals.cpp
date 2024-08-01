#include <math.h>

#include "signals.h"
#include "def.h"
#include "adc.h"
#include "errorManager.h"


Rms::Rms() :
    m_data(nullptr)
{
    m_data = cJSON_CreateObject();
    init();
}

Rms::~Rms()
{
    cJSON_Delete(m_data);
}

void Rms::init()
{
    m_max = -999999.;
    m_mean = 0.;
    m_min = 999999.;
}

void Rms::save(float periodTime, float totalMeasureTime)
{
    float rmsVal = pow(m_temp / periodTime, 0.5);
    m_temp = 0.;

    if (rmsVal < m_min) {
        m_min = rmsVal;
    }
    if (rmsVal > m_max) {
        m_max = rmsVal;
    }

    m_mean = (m_mean * totalMeasureTime + rmsVal * periodTime) / (totalMeasureTime + periodTime);
}

void Rms::update(float val, float deltaT)
{
    m_temp += pow(val, 2) * deltaT;
}


cJSON* Rms::getData()
{
    cJSON_AddNumberToObject(m_data, "min", m_min);
    cJSON_AddNumberToObject(m_data, "mean", m_mean);
    cJSON_AddNumberToObject(m_data, "max", m_max);

    return m_data;
}



Signal::Signal() :
    m_buffer(NB_SAMPLES),
    m_iBuffer(0),
    m_adcChannel(0),
    m_calibCoeffA(0),
    m_calibCoeffB(0),
    m_data(nullptr)
{
    m_data = cJSON_CreateObject();
}

Signal::~Signal()
{
    cJSON_Delete(m_data);
}

void Signal::setChannelId(uint8_t adcChannel)
{
    m_adcChannel = adcChannel;
    m_calibCoeffA = CALIB_A_COEFFS[adcChannel];
    m_calibCoeffB = CALIB_B_COEFFS[adcChannel];
}

void Signal::init()
{
    m_val = 0.;
    m_prevVal = 0.;
    m_maxVal = -999999;
    m_minVal = 999999.;
    m_rms.init();
}

void Signal::setVal(float val)
{
    m_prevVal = m_val;
    m_val = val;

    m_buffer[m_iBuffer] = val;
    m_iBuffer++;
    if (m_iBuffer == NB_SAMPLES) {
        m_iBuffer = 0;
    }
}

cJSON* Signal::getData()
{
    cJSON_AddNumberToObject(m_data, "min", m_minVal);
    cJSON_AddNumberToObject(m_data, "max", m_maxVal);

    return m_data;
}


Tension::Tension() :
    m_data(nullptr),
    m_freqData(nullptr)
{
    setChannelId(TENSION_ID);
    m_data = cJSON_CreateObject();
    m_freqData = cJSON_CreateObject();
    init();
}

Tension::~Tension()
{
    cJSON_Delete(m_freqData);
    cJSON_Delete(m_data);
}

void Tension::init()
{
    Signal::init();
    m_freqMean = 0.;
    m_freqMin = 999999.;
    m_freqMax = 0.;
}

void Tension::calcPeriod(float periodTime, float totalMeasureTime)
{
    float freq = 1. / periodTime;
    if (freq < m_freqMin) {
        m_freqMin = freq;
    }
    if (freq > m_freqMax) {
        m_freqMax = freq;
    }
    m_freqMean = (m_freqMean * totalMeasureTime + freq * periodTime) / (totalMeasureTime + periodTime);
    m_rms.save(periodTime, totalMeasureTime);
}

bool Tension::isCrossingZero(float* czPoint)
{
    if (m_val >= 0. && m_prevVal < 0.) {
        // Linear interpolation to find the exact index where the tension curve is crossing zero
        float a = m_val - m_prevVal;
        float b = m_prevVal;
        *czPoint = -b / a;

        if (*czPoint > 1 || *czPoint < 0) {
            errorManager.error(IMPOSSIBLE_VALUE_ERROR, "Tension", "Error on crossing-zero point : " + std::to_string(*czPoint ));
        }

        return true;
    }
    else {
        return false;
    }
}

void Tension::calcSample(float deltaT, bool lastSample)
{
    if (!lastSample) {
        if (m_val > m_maxVal) {
            m_maxVal = m_val;
        }
        else if (m_val < m_minVal) {
            m_minVal = m_val;
        }
        m_rms.update(m_val, deltaT);
    }
}

cJSON* Tension::getData()
{    
    cJSON_AddNumberToObject(m_freqData, "min", m_freqMin);
    cJSON_AddNumberToObject(m_freqData, "mean", m_freqMean);
    cJSON_AddNumberToObject(m_freqData, "max", m_freqMax);

    cJSON_AddItemToObject(m_data, "RMS(V)", m_rms.getData());
    cJSON_AddItemToObject(m_data, "Range(V)", Signal::getData());
    cJSON_AddItemToObject(m_data, "Frequency(Hz)", m_freqData);

    return m_data;
}


Current::Current() :
    m_data(nullptr)
{
    m_data = cJSON_CreateObject();
    init();
}

Current::~Current()
{
    cJSON_Delete(m_data);
}

void Current::init()
{
    Signal::init();
    m_energy = 0.;
}

void Current::calcSample(float deltaT, bool lastSample)
{
    m_prevVal = m_val;
    m_val = m_calibCoeffA * ((float)adcValues[m_adcChannel] - (float)adcValues[VREF_ID]) + m_calibCoeffB;
    float I = m_val;

    if (lastSample) {
        // If the calculation is the last sample, then make a linear interpolation to get the current corresponding to deltaT
        I = m_prevVal + (m_val - m_prevVal) * deltaT;
        // For the last sample, energy is not updated since U is considered as nul
    }
    else {
        m_energy += m_val * I;
    }

    if (m_val > m_maxVal) {
        m_maxVal = m_val;
    }
    else if (m_val < m_minVal) {
        m_minVal = m_val;
    }
    m_rms.update(I, deltaT);
}

cJSON* Current::getData()
{
    cJSON_AddItemToObject(m_data, "RMS(A)", m_rms.getData());
    cJSON_AddItemToObject(m_data, "Range(A)", Signal::getData());
    cJSON_AddNumberToObject(m_data, "Energy(W)", m_energy);

    return m_data;
}
