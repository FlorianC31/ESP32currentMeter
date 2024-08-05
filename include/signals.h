#ifndef __SIGNALS_H
#define __SIGNALS_H

#include <stdint.h>
#include <vector>
#include <cJSON.h>


struct RangeData {
    float min;
    float max;
    float mean;
};


class Rms
{
public:
    Rms();
    ~Rms() {};
    void init();
    void save(float periodTime, float totalMeasureTime);
    void update(float val, float deltaT);
    cJSON* getJson();
    RangeData getData() {return RangeData(m_min, m_max, m_mean);}

private:
    float m_mean;
    float m_max;
    float m_min;
    float m_temp;
};


class Signal
{
public:
    Signal();
    ~Signal() {};
    virtual void init();
    void setChannelId(uint8_t adcChannel);
    void setVal(float val);
    float getVal() {return m_val;}
    virtual cJSON* getJson();
    static cJSON* serializeData(RangeData &data);
    RangeData getData() {return RangeData({m_minVal, m_maxVal, 0.});}

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
    struct Data {
        RangeData rms;
        RangeData range;
        float energy;
    };

    Current();
    ~Current() {};
    void init() override;
    void calcSample(float deltaT, bool lastSample);
    void calcPeriod(float periodTime, float totalMeasureTime) {m_rms.save(periodTime, totalMeasureTime);}
    cJSON* getJson() override;
    static cJSON* serializeData(Data &data);
    Data getData() {return Data({m_rms.getData(), Signal::getData(), m_energy});}


private:
    float m_energy;          // I*U.dt (W.s)
};

class Tension : public Signal
{
public:
    struct Data {
        RangeData rms;
        RangeData range;
        RangeData freq;
    };

    Tension();
    ~Tension() {};
    void init() override;
    bool isCrossingZero(float* czPoint);
    void calcSample(float deltaT, bool lastSample);
    void calcPeriod(float periodTime, float totalMeasureTime);
    cJSON* getJson() override;
    static cJSON* serializeData(Data &data);
    Data getData() {return Data({m_rms.getData(), Signal::getData(), RangeData({m_freqMin, m_freqMax, m_freqMean})});}

private:
    float m_freqMean;
    float m_freqMax;
    float m_freqMin;
};


#endif      // __SIGNALS_H
