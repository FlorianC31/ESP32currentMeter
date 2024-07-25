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
Chrono::Chrono(std::string name, int printFreq) :
    m_name("Chrono" + name),
    m_printFreq(printFreq)
{
    init();
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
    int64_t end_time = esp_timer_get_time();

    int64_t adc_conversion_time = end_time - m_startTime;
    if (adc_conversion_time > m_maxTime) {
        m_maxTime = adc_conversion_time;
    }
    if (adc_conversion_time < m_minTime) {
        m_minTime = adc_conversion_time;
    }
    m_meanTime = (m_meanTime * m_iter + adc_conversion_time) / (m_iter + 1);

    m_iter++;
    if (m_iter == m_printFreq) {
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
    ESP_LOGI(m_name.c_str(), "min: %lld µs - max: %lld µs - mean: %lld µs", m_minTime, m_maxTime, m_meanTime);
}
