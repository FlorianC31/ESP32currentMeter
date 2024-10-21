#include "chrono.h"
#include "def.h"

#include <esp_log.h>
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
Chrono::Chrono(std::string name, int limit, int printFreq, bool debug) :
    m_name("Chrono" + name),
    m_limit(limit),
    m_debug(debug),
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
    m_freq.max = 0;
    m_freq.min = 1000000;
    m_freq.mean = 0;

    m_duration.max = 0;
    m_duration.min = 1000000;
    m_duration.mean = 0;
    
    m_cpuUsage.max = 0;
    m_cpuUsage.min = 1000000;
    m_cpuUsage.mean = 0;
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
        return;
    }

    int end_time = esp_timer_get_time();    // µs
    m_curentFreq = 1000000 / end_time;          // Hz
    calc(&m_freq, m_curentFreq);
}



/**
 * @brief Ends a timing cycle.
 *
 * Calculates the elapsed time since the start of the cycle, updates the
 * timing statistics, and prints the statistics if the print frequency is reached.
 */
void Chrono::endCycle()
{
    if (m_startTime == 0) {
        return;
    }

    int end_time = esp_timer_get_time();

    int adc_conversion_time = end_time - m_startTime;
    calc(&m_duration, static_cast<float>(adc_conversion_time));

    float cpuUsage = adc_conversion_time * m_curentFreq / 10000.;
    calc(&m_cpuUsage, cpuUsage);

    m_iter++;
    if (m_iter == m_printFreq) {
        calcGlobal();
        if (m_debug) {
            print();
        }
        init();
    }
}



void Chrono::calc(Data* data, float value)
{
    if (value > data->max) {
        data->max = value;
    }
    if (value < data->min) {
        data->min = value;
    }
    if (value > data->limit) {
        data->nbOverDeadline++;
    }
    data->mean = (data->mean * m_iter + value) / (m_iter + 1);
}

/**
 * @brief Calcul the global timing statistics (for API GET)
 *
 * Logs the global minimum, maximum, and mean times of the cycles.
 */
void Chrono::calcGlobal()
{
    if (m_duration.max > m_duration.globalMax) {
        m_duration.globalMax = m_duration.max;
    }
    if (m_duration.min < m_duration.globalMin) {
        m_duration.globalMin = m_duration.min;
    }
    m_duration.globalMean = (m_duration.globalMean * m_totalIter + m_duration.mean) / (m_totalIter + 1);

    
    if (m_freq.max > m_freq.globalMax) {
        m_freq.globalMax = m_freq.max;
    }
    if (m_freq.min < m_freq.globalMin) {
        m_freq.globalMin = m_freq.min;
    }
    m_freq.globalMean = (m_freq.globalMean * m_totalIter + m_freq.mean) / (m_totalIter + 1);

    
    if (m_cpuUsage.max > m_cpuUsage.globalMax) {
        m_cpuUsage.globalMax = m_cpuUsage.max;
    }
    if (m_cpuUsage.min < m_cpuUsage.globalMin) {
        m_cpuUsage.globalMin = m_cpuUsage.min;
    }
    m_cpuUsage.globalMean = (m_cpuUsage.globalMean * m_totalIter + m_cpuUsage.mean) / (m_totalIter + 1);
    m_totalIter++;
}

/**
 * @brief Prints the timing statistics.
 *
 * Logs the minimum, maximum, and mean times of the cycles.
 */
void Chrono::print()
{
    ESP_LOGI(m_name.c_str(), "min: %fµs - max: %fµs - mean: %fµs", m_duration.min, m_duration.max, m_duration.mean);
    ESP_LOGI(m_name.c_str(), "min: %fHz - max: %fHz - mean: %fHZ", m_freq.min, m_freq.max, m_freq.mean);
    ESP_LOGI(m_name.c_str(), "min: %f%% - max: %f%% - mean: %f%%", m_cpuUsage.min, m_cpuUsage.max, m_cpuUsage.mean);
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
    cJSON_AddNumberToObject(json, "ElapsedTime(s)", m_totalIter);

    cJSON* jsonFreq = cJSON_CreateObject();
    cJSON_AddNumberToObject(jsonFreq, "min(Hz)", m_freq.globalMin);
    cJSON_AddNumberToObject(jsonFreq, "mean(Hz)", m_freq.globalMean);
    cJSON_AddNumberToObject(jsonFreq, "max(Hz)", m_freq.globalMax);
    cJSON_AddNumberToObject(jsonFreq, std::string("nbOver " + std::to_string(m_limit) +"µs(%)").c_str(), (float)m_freq.nbOverDeadline / (m_totalIter * m_printFreq) * 100);
    cJSON_AddItemToObject(json, "Frequency", jsonFreq);

    cJSON* jsonDuration = cJSON_CreateObject();
    cJSON_AddNumberToObject(jsonDuration, "min(µs)", m_duration.globalMin);
    cJSON_AddNumberToObject(jsonDuration, "mean(µs)", m_duration.globalMean);
    cJSON_AddNumberToObject(jsonDuration, "max(µs)", m_duration.globalMax);
    cJSON_AddNumberToObject(jsonDuration, std::string("nbOver " + std::to_string(m_limit) +"µs(%)").c_str(), (float)m_duration.nbOverDeadline / (m_totalIter * m_printFreq) * 100);
    cJSON_AddItemToObject(json, "Task Duration", jsonDuration);

    cJSON* jsonCpuUsage = cJSON_CreateObject();
    cJSON_AddNumberToObject(jsonCpuUsage, "min(%)", m_cpuUsage.globalMin);
    cJSON_AddNumberToObject(jsonCpuUsage, "mean(%)", m_cpuUsage.globalMean);
    cJSON_AddNumberToObject(jsonCpuUsage, "max(%)", m_cpuUsage.globalMax);
    cJSON_AddNumberToObject(jsonCpuUsage, std::string("nbOver " + std::to_string(m_limit) +"µs(%)").c_str(), (float)m_duration.nbOverDeadline / (m_totalIter * m_printFreq) * 100);
    cJSON_AddItemToObject(json, "CPU Usage", jsonCpuUsage);

    std::string globalStats = cJSON_Print(json);
    cJSON_Delete(json); 

    return globalStats;
}
