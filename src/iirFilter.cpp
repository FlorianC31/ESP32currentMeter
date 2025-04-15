#include "iirFilter.h"


IIRFilter::IIRFilter() :
    missingInputs(FILTER_ORDER)
{
    // Initialize the filter coefficients for a 2nd order IIR filter
    float alpha = std::exp(-2 * M_PI * CUTOFF_FREQ / SAMPLE_FREQ);
    float k = 1. - alpha;
    a[1] = std::pow(alpha, 2.) * -1.;
    a[0] = 2. * alpha;

    b[2] = std::pow(k, 2.) * std::pow(alpha, 2.);
    b[1] = 2. * std::pow(k, 2.) * alpha;
    b[0] = std::pow(k, 2.);

    // Normalize the coefficients
    // The sum of the coefficients should be 1 for a proper filter
    float sum = a[0] + a[1] + b[0] + b[1] + b[2];
    a[0] /= sum;
    a[1] /= sum;
    b[0] /= sum;
    b[1] /= sum;
    b[2] /= sum;
}


float IIRFilter::process(float input) {
    // Update the x buffer with the new input sample
    x[2] = x[1];
    x[1] = x[0];
    x[0] = input;
    float newY = 0.;

    if (missingInputs > 0) {
        // If we have not received enough inputs yet, just return the input value
        missingInputs--;
        newY = input;
    }
    else {
        // Compute the output sample using the IIR filter formula
        newY = b[0] * x[0] + b[1] * x[1] + b[2] * x[2] + a[0] * y[0] + a[1] * y[1];
    }
    
    // Update the y buffer with the new output sample
    y[1] = y[0];
    y[0] = newY;

    // Return the current output sample
    return newY;
}