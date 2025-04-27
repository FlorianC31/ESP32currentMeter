#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include "def.h"
#include "alternatingBuffer.h"
#include "iirFilter.h"
#include "fft.h"

class ElecSignal
{
public:
    ElecSignal(std::string name, bool isTension, fft_config_t* fftManager, float calibCoeff = 0., ElecSignal* tensionSignal = nullptr);
    virtual ~ElecSignal();

    void addRawData(float data);
    std::string getHarmoniques() const;
    float getLastValue(bool filtered = false) const;
    std::array<float, BUFFER_SIZE>* getData(bool fitered = false) const;
    void runAnalysis();
    bool isReadyForProcessing() {return m_filteredDataBuffer.isBufferReadyForProcessing();}

private:
    void calcFrequency();
    static float calcZeroCrossingIndex(float x1, float y1, float x2, float y2, float zero = 0.);
    void removeOffset();
    float getMeanValue() const;
    void calcRmsValue();


private:
    std::string m_name;                         // Signal name
    bool m_isTension;                           // Is the signal the tension signal?
    float m_calibCoeff;                         // Calibration coefficient
    fft_config_t* m_fftManager = nullptr;       // FFT manager for the signal
    ElecSignal* m_tensionSignal;                // Pointer to the signal of Tension
    AlternatingBuffer m_rawDataBuffer;          // Buffer for raw data
    AlternatingBuffer m_filteredDataBuffer;     // Buffer for filtered data
    IIRFilter m_filter;                         // Filter for the signal

    float m_period;
    float m_firstRisingZeroCrossingIndex;
    float m_lastRisingZeroCrossingIndex;

    float m_frequency;                          // Frequency of the signal (Hz)
    float m_rmsValue;                           // RMS value of the signal (A for current, V for tension)
    float m_maxValue;                           // Maximum value of the signal (A for current, V for tension)

};






#endif // __SIGNAL_H__