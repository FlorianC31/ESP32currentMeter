#ifndef __IIR_FILTER_H
#define __IIR_FILTER_H

#include "def.h"


class IIRFilter {
public:
    IIRFilter();
    ~IIRFilter() {};

    float process(float input); // Process a new input sample and return the filtered output	

private:
    std::array<float, FILTER_ORDER> a; // Coefficients for the feedback part of the filter
    std::array<float, FILTER_ORDER + 1> b; // Coefficients for the feedforward part of the filter
    std::array<float, FILTER_ORDER + 1> x; // Input samples (buffer)
    std::array<float, FILTER_ORDER> y; // Output samples (buffer

    int missingInputs;

};





#endif // __IIR_FILTER_H
