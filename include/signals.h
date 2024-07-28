#ifndef __SIGNALS_H
#define __SIGNALS_H

#include <stdint.h>
#include <vector>

class Rms
{
public:
    Rms() {init();}
    ~Rms() {}
    void init();
    void save(float periodTime, float totalMeasureTime);
    void update(float val, float deltaT);

private :
    float m_temp;
    float m_mean;
    float m_max;
    float m_min;
};


class Signal
{
public:
    Signal(uint8_t adcChannel);
    ~Signal() {}
    virtual void init();
    void setVal(float val);
    float getVal() {return m_val;}

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
};


class Current : public Signal
{
public:
    Current(uint8_t adcChannel);
    ~Current() {}

    void init() override;
    void calcSample(float deltaT, bool lastSample);
    void calcPeriod(float periodTime, float totalMeasureTime) {m_rms.save(periodTime, totalMeasureTime);}

private:
    float m_energy;          // I*U.dt (W.s)
};

class Tension : public Signal
{
public:
    Tension();
    ~Tension() {}

    void init() override;
    bool isCrossingZero(float* czPoint);
    void calcSample(float deltaT, bool lastSample);
    void calcPeriod(float periodTime, float totalMeasureTime);

private:
    float m_freqMean;
    float m_freqMax;
    float m_freqMin;
};


#endif      // __SIGNALS_H
