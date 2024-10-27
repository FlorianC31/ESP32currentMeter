#include "measure.h"
#include "def.h"
#include "errorManager.h"
#include "ntp.h"

Measure::Measure() :
    m_currents(NB_CURRENTS),
    m_data(nullptr),
    m_firstError(true)
{
    m_data = cJSON_CreateObject();
    for (uint8_t i = 0; i < NB_CURRENTS; i++) {
        m_currents[i].setChannelId(i);
    }
    m_calibCoeffA = {CURRENT1_COEF_A, CURRENT2_COEF_A, CURRENT3_COEF_A, CURRENT4_COEF_A, CURRENT5_COEF_A, CURRENT6_COEF_A};
    m_calibCoeffB = {CURRENT1_COEF_B, CURRENT2_COEF_B, CURRENT3_COEF_B, CURRENT4_COEF_B, CURRENT5_COEF_B, CURRENT6_COEF_B};
    init();
}

Measure::~Measure()
{
    cJSON_Delete(m_data);
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


void Measure::cal(std::array<uint16_t, NB_CHANNELS> adcData)
{
    // Compute the a
    m_tension.setVal(TENSION_COEF_A * ((float)adcData[TENSION_ID] - (float)adcData[VREF_ID]) + TENSION_COEF_B);
    for (uint8_t currentId = 0; currentId < NB_CURRENTS; currentId++) {
        m_currents[currentId].setVal(m_calibCoeffA[currentId] * ((float)adcData[currentId] - (float)adcData[VREF_ID]) + m_calibCoeffB[currentId]);
    }
    
    float czPoint(0.);
    float deltaT(0.);

    switch(m_initState) {
        case INIT:
            //ESP_LOGI("Measure", "Current State : %s", "INIT");
            //ESP_LOGI("Measure", "Switching State : %s", "WAITING_ZC");
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
                //ESP_LOGI("Measure", "Switching State : %s", "NORMAL_PHASE");
            }
            return;
        break;

        default:
        case NORMAL_PHASE:
        break;
    }

    if (m_tension.isCrossingZero(&czPoint)) {

        // Robustess check
        if (m_periodTime < (1. / MAX_AC_FREQ) && m_firstError) {
            errorManager.error(AC_FREQ_ERROR, "Measure", "Error on AC frequency calculation : " + std::to_string(1. / m_periodTime));
            m_firstError = false;
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
        if (m_totalMeasureTime > MEASURE_PACKET_PERIOD) {
            save();
            m_totalMeasureTime = 0.;
        }
    }
    else {
        m_tension.calcSample(deltaT, false);
        for(Current &current : m_currents) {
            current.calcSample(deltaT, false);
        }
        m_periodTime += m_timerPeriod;
        
        if (((1. / m_periodTime) < MIN_AC_FREQ) && m_firstError) {
            errorManager.error(AC_FREQ_ERROR, "Measure", "AC Frequency is to low : " + std::to_string(1. / m_periodTime) + "Hz");
            m_firstError = false;
        }
    }
}


std::string Measure::getJsonOld()
{
    cJSON_AddNumberToObject(m_data, "TotalMeasureTime(s)", m_totalMeasureTime);
    cJSON_AddItemToObject(m_data, "Tension", m_tension.getJson());

    for (uint8_t i = 0; i < NB_CURRENTS; i++) {
        cJSON_AddItemToObject(m_data, std::string("Current" + std::to_string(i)).c_str(), m_currents[i].getJson());
    }

    return std::string(cJSON_Print(m_data));
}


/**
 * @brief Save the measure data in a struct and push it into the FIFO
 * 
 */
void Measure::save()
{       
    Data newData;
    newData.timestamp = get_timestamp();
    newData.duration = m_totalMeasureTime;
    newData.tension = m_tension.getData();
    uint8_t i = 0;
    for (Current &current: m_currents) {
        newData.currents[i] = current.getData();
        i++;
    }   

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_fifo.push(newData);
}


cJSON* Measure::serializeData(Measure::Data &data)
{
    cJSON* jsonMeasure = cJSON_CreateObject();
    cJSON_AddNumberToObject(jsonMeasure, "timestamp", data.timestamp);
    cJSON_AddNumberToObject(jsonMeasure, "duration", data.duration);
    cJSON_AddItemToObject(jsonMeasure, "tension", Tension::serializeData(data.tension));

    uint8_t i = 0;
    for (Current::Data &current : data.currents) {
        cJSON_AddItemToObject(jsonMeasure, std::string("current" + std::to_string(i)).c_str(), Current::serializeData(current));
        i++;
    }  

    return jsonMeasure;
}


bool Measure::popFromQueue(Measure::Data &data)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (!m_fifo.empty()) {
        data = m_fifo.front();
        m_fifo.pop();
        return true;
    }
    return false;
}


std::string Measure::getJson()
{

    Data data;
    cJSON* jsonDataList = cJSON_CreateArray();

    while (popFromQueue(data)) {
        cJSON* jsonData = serializeData(data);
        cJSON_AddItemToArray(jsonDataList, jsonData);
    }

    std::string dataString(cJSON_Print(jsonDataList));
    cJSON_Delete(jsonDataList);

    return dataString;
}

