#ifndef __SIGNALS_H
#define __SIGNALS_H

#include <stdint.h>
#include <vector>
#include <cJSON.h>

class Rms
{
public:
    Rms();
    ~Rms();
    void init();
    void save(float periodTime, float totalMeasureTime);
    void update(float val, float deltaT);
    cJSON* getData();

private :
    float m_temp;
    float m_mean;
    float m_max;
    float m_min;
    cJSON* m_data;
};


class Signal
{
public:
    Signal();
    ~Signal();
    virtual void init();
    void setChannelId(uint8_t adcChannel);
    void setVal(float val);
    float getVal() {return m_val;}
    virtual cJSON* getData();

protected:
    float m_val;
    float m_prevVal;
    float m_maxVal;
    float m_minVal;
    Rms m_rms;
    std::vector<float> m_buffer;
    uint16_t m_iBuffer;
    uint8_t m_adcChannel;
    float m_calibCoeffA;
    float m_calibCoeffB;

private:
    cJSON* m_data;
};


class Current : public Signal
{
public:
    Current();
    ~Current();
    void init() override;
    void calcSample(float deltaT, bool lastSample);
    void calcPeriod(float periodTime, float totalMeasureTime) {m_rms.save(periodTime, totalMeasureTime);}
    cJSON* getData() override;

private:
    float m_energy;          // I*U.dt (W.s)
    cJSON* m_data;
};

class Tension : public Signal
{
public:
    Tension();
    ~Tension();
    void init() override;
    bool isCrossingZero(float* czPoint);
    void calcSample(float deltaT, bool lastSample);
    void calcPeriod(float periodTime, float totalMeasureTime);
    cJSON* getData() override;

private:
    float m_freqMean;
    float m_freqMax;
    float m_freqMin;
    cJSON* m_data;
    cJSON* m_freqData;
};


#endif      // __SIGNALS_H
