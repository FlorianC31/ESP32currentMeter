#include "chrono.h"

#include <esp_log.h>
#include <esp_timer.h>


/**
 * @brief Constructor for the Chrono class.
 *
 * Initializes the Chrono object with a name and a print frequency.
 *
 * @param name The name of the Chrono object.
 * @param printFreq The frequency at which to print the timing statistics.
 */
Chrono::Chrono(std::string name, int limit, int printFreq, bool debug) :
    m_name("Chrono" + name),
    m_limit(limit),
    m_debug(debug),
    m_printFreq(printFreq),
    m_nbOverLimit(0),
    m_globalStats(nullptr)
{
    m_globalStats = cJSON_CreateObject();
    init();
}


Chrono::~Chrono()
{
    cJSON_Delete(m_globalStats);
    if (m_globalStats) {
        delete m_globalStats;
    }
}

/**
 * @brief Initializes the Chrono object.
 *
 * Resets the iteration count, maximum time, minimum time, and mean time.
 */
void Chrono::init()
{
    m_iter = 0;
    m_maxTime = 0;
    m_minTime = 1000000;
    m_meanTime = 0;
}

/**
 * @brief Starts a timing cycle.
 *
 * Records the start time of the cycle.
 */
void Chrono::startCycle()
{
    m_startTime = esp_timer_get_time();
}

/**
 * @brief Ends a timing cycle.
 *
 * Calculates the elapsed time since the start of the cycle, updates the
 * timing statistics, and prints the statistics if the print frequency is reached.
 */
void Chrono::endCycle()
{
    int end_time = esp_timer_get_time();

    int adc_conversion_time = end_time - m_startTime;
    if (adc_conversion_time > m_maxTime) {
        m_maxTime = adc_conversion_time;
    }
    if (adc_conversion_time < m_minTime) {
        m_minTime = adc_conversion_time;
    }
    if (adc_conversion_time > m_limit) {
        m_nbOverLimit++;
    }
    m_meanTime = (m_meanTime * m_iter + adc_conversion_time) / (m_iter + 1);
    m_iter++;
    if (m_iter == m_printFreq) {
        calcGlobal();
        if (m_debug) {
            print();
        }
        init();
    }
}

/**
 * @brief Calcul the global timing statistics (for API GET)
 *
 * Logs the global minimum, maximum, and mean times of the cycles.
 */
void Chrono::calcGlobal()
{
    if (m_maxTime > m_totalMaxTime) {
        m_totalMaxTime = m_maxTime;
    }
    if (m_minTime < m_totalMinTime) {
        m_totalMinTime = m_minTime;
    }
    m_totalMeanTime = (m_totalMeanTime * m_totalIter + m_meanTime) / (m_totalIter + 1);
    m_totalIter++;
}

/**
 * @brief Prints the timing statistics.
 *
 * Logs the minimum, maximum, and mean times of the cycles.
 */
void Chrono::print()
{
    ESP_LOGI(m_name.c_str(), "min: %iµs - max: %iµs - mean: %iµs", m_minTime, m_maxTime, m_meanTime);
}

/**
 * @brief get the global chrono statistics
 * 
 * @return std::string json global statistics
 */
std::string Chrono::getGlobalStats()
{
    cJSON_AddStringToObject(m_globalStats, "Chrono", m_name.c_str());
    cJSON_AddNumberToObject(m_globalStats, "ElapsedTime(s)", m_totalIter);
    cJSON_AddNumberToObject(m_globalStats, "min(µs)", m_totalMinTime);
    cJSON_AddNumberToObject(m_globalStats, "mean(µs)", m_totalMeanTime);
    cJSON_AddNumberToObject(m_globalStats, "max(µs)", m_totalMaxTime);
    cJSON_AddNumberToObject(m_globalStats, std::string("nbOver" + std::to_string(m_limit) +"µs(%)").c_str(), (float)m_nbOverLimit / (m_totalIter * m_printFreq) * 100);

    char *json_string = cJSON_Print(m_globalStats);

    return std::string(json_string);
}
