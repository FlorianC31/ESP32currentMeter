#include "chrono.h"
#include "def.h"

#include <esp_timer.h>


/**
 * @brief Constructor for the Chrono class.
 *
 * Initializes the Chrono object with a name and a print frequency.
 *
 * @param name The name of the Chrono object.
 * @param limit high limit (in µs) of the chrono (a counter will count how many times this limit is not respected)
 * @param printFreq The frequency (number of itterations) at which to print the timing statistics.
 * @param debug if true, the chrono will be printed at the end if each cycle
 */
Chrono::Chrono(std::string name, int limitFreq, float limitCpu, int nbIgnored, int printFreq) :
    m_name("Chrono" + name),
    m_limitFreq(limitFreq),
    m_limitCpu(limitCpu),
    m_nbIgnored(nbIgnored),
    m_printFreq(printFreq),
    m_startTime(0),
    m_lastStartTime(0)
{
    init();
}


Chrono::~Chrono()
{}

/**
 * @brief Initializes the Chrono object.
 *
 * Resets the iteration count, maximum time, minimum time, and mean time.
 */
void Chrono::init()
{
    m_iter = 0;
    m_freq.init(m_limitFreq, true);
    m_cpuUsage.init(m_limitCpu);
    int limitDuration = m_limitCpu * 10000 / m_limitFreq;
    m_duration.init(limitDuration);
}

/**
 * @brief Starts a timing cycle.
 *
 * Records the start time of the cycle.
 */
void Chrono::startCycle()
{
    m_startTime = esp_timer_get_time();

    if (m_lastStartTime == 0) {
        m_lastStartTime = m_startTime;
        return;
    }

    m_curentFreq = 1000000. / (m_startTime - m_lastStartTime);          // Hz
    m_lastStartTime = m_startTime;

    if (m_nbIgnored <= 0) {
        m_freq.add(m_curentFreq, m_iter);
    }
    else {
        m_nbIgnored--;
    }
}



/**
 * @brief Ends a timing cycle.
 *
 * Calculates the elapsed time since the start of the cycle, updates the
 * timing statistics, and prints the statistics if the print frequency is reached.
 */
void Chrono::endCycle()
{
    if (m_startTime == 0 || m_nbIgnored > 0) {
        return;
    }

    

    int end_time = esp_timer_get_time();
    int adc_conversion_time = end_time - m_startTime;
    m_duration.add(adc_conversion_time, m_iter);


    float cpuUsage = adc_conversion_time * m_curentFreq / 10000.;
    m_cpuUsage.add(cpuUsage, m_iter);

    m_iter++;

    if (m_printFreq != 0 && m_iter == m_printFreq) {
        print();
        init();
    }
}


/**
 * @brief Prints the timing statistics.
 *
 * Logs the minimum, maximum, and mean times of the cycles.
 */
void Chrono::print()
{
    /*m_duration.print(m_name, "µs");
    m_freq.print(m_name, "Hz");
    m_cpuUsage.print(m_name, "%");*/
}

/**
 * @brief get the global chrono statistics
 * 
 * @return std::string json global statistics
 */
std::string Chrono::getGlobalStats()
{

    cJSON *json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "Chrono", m_name.c_str());
    cJSON_AddNumberToObject(json, "Number of iterations", m_iter);

    cJSON_AddItemToObject(json, "Frequency", m_freq.getJson("Hz", m_iter));
    cJSON_AddItemToObject(json, "Task Duration", m_duration.getJson("µs", m_iter));
    cJSON_AddItemToObject(json, "CPU Usage", m_cpuUsage.getJson("%", m_iter));

    std::string globalStats = cJSON_Print(json);
    cJSON_Delete(json); 

    return globalStats;
}
