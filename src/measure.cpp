#include "measure.h"
#include "errorManager.h"



Measure::Measure()
{
    for (uint8_t i = 0; i < NB_CURRENTS; i++) {
        m_currents.push_back(Current(i));
    }
    init();
}


void Measure::init()
{
    m_timerPeriod = TIM_PERIOD / 1000000.;    // seconds
    float freq = 1. / m_timerPeriod;

    if (freq > MAX_AC_FREQ && (freq < MIN_AC_FREQ)) {
        errorManager.error(INIT_ERROR, "Measure", "Error on frequency initialization: " + std::to_string(freq));
    }
 
    m_tension.init();
    for (uint8_t i = 0; i != NB_CURRENTS; i++) {
        m_currents[i].init();
    }

    m_initState = INIT;
}


void Measure::adcCallback(uint32_t* data)
{
    //m_periodTimeBuffer[m_iPeriodTimeBuffer] = m_periodTime;
    //m_iPeriodTimeBuffer++;

    m_tension.setVal(CALIB_A_COEFFS[TENSION_ID] * ((float)data[TENSION_ID] - (float)data[VREF_ID]) + CALIB_B_COEFFS[TENSION_ID]);
    
    float czPoint(0.);
    float deltaT(0.);

    switch(m_initState) {
        case INIT:
            m_initState = WAITING_ZC;
            return;
        break;

        case WAITING_ZC:
            if (m_tension.isCrossingZero(&czPoint)) {
                deltaT = m_timerPeriod * (1. - czPoint);
                m_totalMeasureTime = 0.;
                m_tension.calcSample(deltaT, false);
                for(Current &current : m_currents) {
                    current.calcSample(deltaT, false);
                }
                m_periodTime = deltaT;
                m_initState = NORMAL_PHASE;
            }
            return;
        break;

        default:
        case NORMAL_PHASE:
        break;
    }

    if (m_tension.isCrossingZero(&czPoint)) {

        // Robustess check
        if (m_periodTime < (1. / MAX_AC_FREQ)) {
            errorManager.error(AC_FREQ_ERROR, "Measure", "Error on AC frequency calculation : " + std::to_string(1. / m_periodTime));
        }

        // Calculation of the last point of the previous period
        deltaT = m_timerPeriod * czPoint;
        m_tension.calcSample(deltaT, false);
        for(Current &current : m_currents) {
            current.calcSample(deltaT, false);
        }
        
        // update current time with the last step of the previous period
        m_periodTime += deltaT;

        // Calculation of the complete previous period
        m_tension.calcPeriod(m_periodTime, m_totalMeasureTime);
        for(Current &current : m_currents) {
            current.calcPeriod(m_periodTime, m_totalMeasureTime);
        }

        // add the last period time to the the total Measure Time
        m_totalMeasureTime += m_periodTime;
        
        // Calculation of the first point of the new period
        deltaT = m_timerPeriod * (1. - czPoint);
        m_tension.calcSample(deltaT, false);
        for(Current &current : m_currents) {
            current.calcSample(deltaT, false);
        }

        // initialize current time with the first step of the new period
        m_periodTime = deltaT;
        
        // After 5 minutes, send the set of data through the UART and reset the data set
        if (m_totalMeasureTime > DATA_SENT_PERIOD) {
            //sendEnergyData();
            m_totalMeasureTime = 0.;
        }
    }
    else {
        m_tension.calcSample(deltaT, false);
        for(Current &current : m_currents) {
            current.calcSample(deltaT, false);
        }
        m_periodTime += m_timerPeriod;
        
        if (m_periodTime > (1. / MIN_AC_FREQ)) {
            errorManager.error(AC_FREQ_ERROR, "Measure", "Error on AC frequency calculation : " + std::to_string(1. / m_periodTime));
        }
    }
}






void sendEnergyData()
{
    bool stop = true;
    UNUSED(stop);
}



