#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include "def.h"
#include "alternatingBuffer.h"
#include "iirFilter.h"
#include "fft.h"

class ElecSignal
{
public:
    ElecSignal(std::string name, fft_config_t* fftManager, float calibCoeff = 0., ElecSignal* refSignal = nullptr, ElecSignal* tensionSignal = nullptr);
    virtual ~ElecSignal();

    void addRawData(float data);
    std::string getHarmoniques() const;
    float getLastValue(bool filtered = false) const;
    std::array<float, BUFFER_SIZE>* getData(bool fitered = false) const;
    void runAnalysis(bool isTension = false);
    bool isReadyForProcessing() {return m_filteredDataBuffer.isBufferReadyForProcessing();}

private:
    float calcFrequency();
    static float calcZeroCrossingIndex(float x1, float y1, float x2, float y2, float zero = 0.);
    void removeOffset();
    float getMeanValue() const;


private:
    std::string m_name;                         // Signal name
    float m_calibCoeff;                         // Calibration coefficient
    fft_config_t* m_fftManager = nullptr;       // FFT manager for the signal
    ElecSignal* m_refSignal;                    // Pointer to the signal of Vref
    ElecSignal* m_tensionSignal;                // Pointer to the signal of Tension
    AlternatingBuffer m_rawDataBuffer;          // Buffer for raw data
    AlternatingBuffer m_filteredDataBuffer;     // Buffer for filtered data
    IIRFilter m_filter;                         // Filter for the signal

    float m_period;

};






#endif // __SIGNAL_H__