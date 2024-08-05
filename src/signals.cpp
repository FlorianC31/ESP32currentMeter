#include <math.h>

#include "signals.h"
#include "def.h"
#include "adc.h"
#include "errorManager.h"


Rms::Rms()
{
    init();
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


cJSON* Rms::getJson()
{
    cJSON* data = cJSON_CreateObject();

    cJSON_AddNumberToObject(data, "min", m_min);
    cJSON_AddNumberToObject(data, "mean", m_mean);
    cJSON_AddNumberToObject(data, "max", m_max);

    return data;
}



Signal::Signal() :
    m_buffer(NB_SAMPLES),
    m_iBuffer(0),
    m_adcChannel(0),
    m_calibCoeffA(0),
    m_calibCoeffB(0)
{}


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

cJSON* Signal::getJson()
{
    cJSON* data = cJSON_CreateObject();

    cJSON_AddNumberToObject(data, "min", m_minVal);
    cJSON_AddNumberToObject(data, "max", m_maxVal);

    return data;
}


cJSON* Signal::serializeData(RangeData &data)
{
    cJSON* jsonData = cJSON_CreateObject();
    cJSON_AddNumberToObject(jsonData, "min", data.min);
    cJSON_AddNumberToObject(jsonData, "mean", data.mean);
    cJSON_AddNumberToObject(jsonData, "max", data.max);

    return jsonData;
}


Tension::Tension()
{
    setChannelId(TENSION_ID);
    init();
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

cJSON* Tension::getJson()
{    
    cJSON* freqData = cJSON_CreateObject();
    cJSON_AddNumberToObject(freqData, "min", m_freqMin);
    cJSON_AddNumberToObject(freqData, "mean", m_freqMean);
    cJSON_AddNumberToObject(freqData, "max", m_freqMax);

    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "RMS(V)", m_rms.getJson());
    cJSON_AddItemToObject(data, "range(V)", Signal::getJson());
    cJSON_AddItemToObject(data, "frequency(Hz)", freqData);

    return data;
}

cJSON* Tension::serializeData(Tension::Data &data)
{
    cJSON* jsonData = cJSON_CreateObject();
    cJSON_AddItemToObject(jsonData, "RMS(V)", Signal::serializeData(data.rms));
    cJSON_AddItemToObject(jsonData, "range(V)", Signal::serializeData(data.range));
    cJSON_AddItemToObject(jsonData, "frequency(Hz)", Signal::serializeData(data.freq));

    return jsonData;
}





Current::Current()
{
    init();
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
        m_energy += m_val * I * TIM_PERIOD / 3600;      // The energy is in Wh
    }

    if (m_val > m_maxVal) {
        m_maxVal = m_val;
    }
    else if (m_val < m_minVal) {
        m_minVal = m_val;
    }
    m_rms.update(I, deltaT);
}

cJSON* Current::getJson()
{
    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "RMS(A)", m_rms.getJson());
    cJSON_AddItemToObject(data, "Range(A)", Signal::getJson());
    cJSON_AddNumberToObject(data, "Energy(W.h)", m_energy);

    return data;
}

cJSON* Current::serializeData(Current::Data &data)
{
    cJSON* jsonData = cJSON_CreateObject();
    cJSON_AddItemToObject(jsonData, "RMS(A)", Signal::serializeData(data.rms));
    cJSON_AddItemToObject(jsonData, "Range(A)", Signal::serializeData(data.range));
    cJSON_AddNumberToObject(jsonData, "Energy(W.h)", data.energy);

    return jsonData;
}
