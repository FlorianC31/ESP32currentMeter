#include "elecSignal.h"



ElecSignal::ElecSignal(std::string name, fft_config_t* fftManager, float calibCoeff, ElecSignal* refSignal, ElecSignal* tensionSignal) :
    m_name(name),
    m_calibCoeff(calibCoeff),
    m_fftManager(fftManager),
    m_refSignal(refSignal),
    m_tensionSignal(tensionSignal)
{
    
    multi_heap_info_t info;

    ESP_LOGI(m_name.c_str(), "#######  Start of intialization ########");
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGW(m_name.c_str(), "Allocated heap size (kB): %f", float(info.total_allocated_bytes) / 1000.);

    // Initialize the raw data buffer and filtered data buffer
    m_rawDataBuffer.reset();
    m_filteredDataBuffer.reset();

    ESP_LOGI(m_name.c_str(), "Initializing IIR filter");
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGW(m_name.c_str(), "Allocated heap size (kB): %f", float(info.total_allocated_bytes) / 1000.);

    // Initialize the filter
    m_filter = IIRFilter();

    ESP_LOGI(m_name.c_str(), "End of initialization");
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGW(m_name.c_str(), "Allocated heap size (kB): %f", float(info.total_allocated_bytes) / 1000.);

    ESP_LOGI("", "");

}


ElecSignal::~ElecSignal()
{
    // Clean up the FFT plan
    if (m_fftManager != nullptr) {
        fft_destroy(m_fftManager);
        m_fftManager = nullptr;
    }
}


void ElecSignal::addRawData(float data)
{
    float filteredData = m_filter.process(data);
    if (m_refSignal != nullptr) {
        // Store the raw data and filtered data in their respective buffers, adjusted by the reference signal
        m_rawDataBuffer.addData(data - m_refSignal->getLastValue());
        m_filteredDataBuffer.addData(filteredData - m_refSignal->getLastValue(true));
    } else {
        // Store the raw data and filtered data in their respective buffers without adjustment
        m_rawDataBuffer.addData(data);
        m_filteredDataBuffer.addData(filteredData);
    }
}


void ElecSignal::runAnalysis(bool isTension)
{

    /*m_fftManager->input = m_rawDataBuffer.getData()->data();
    fft_execute(m_fftManager);*/

    // Process the data in the buffer if it's ready
    if (isTension) {
        /*ESP_LOGE("ElecSignal", "%f,%f,%f,%f,%f,[...],%f",
            m_filteredDataBuffer.getData()->at(0),
            m_filteredDataBuffer.getData()->at(1),
            m_filteredDataBuffer.getData()->at(2),
            m_filteredDataBuffer.getData()->at(3),
            m_filteredDataBuffer.getData()->at(4),
            m_filteredDataBuffer.getData()->at(BUFFER_SIZE - 1)
        );*/
        calcFrequency();
    }
    
}


float ElecSignal::getLastValue(bool filtered) const
{
    if (filtered) {
        return m_filteredDataBuffer.getLastValue();
    } else {
        return m_rawDataBuffer.getLastValue();
    }
}


std::array<float, BUFFER_SIZE>* ElecSignal::getData(bool fitered) const {
    if (fitered) {
        return m_filteredDataBuffer.getData();
    } else {
        return m_rawDataBuffer.getData();
    }
}


float ElecSignal::getMeanValue() const
{
    float meanValue = 0.;
    for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
        meanValue += m_rawDataBuffer.getData()->at(i);
    }
    meanValue /= BUFFER_SIZE;
    return meanValue;
}

void ElecSignal::removeOffset()
{

}


/**
 * * @brief Calculate the zero crossing index between two points
 * * @param x1 First x coordinate
 * * @param y1 First y coordinate
 * * @param x2 Second x coordinate
 * * @param y2 Second y coordinate
 * * @param zero The zero value to cross (default is 0.)
 * * @return The zero crossing index, or -1 if invalid
 * * @details
 * This function calculates the zero crossing index between two points (x1, y1) and (x2, y2).
 */
float ElecSignal::calcZeroCrossingIndex(float x1, float y1, float x2, float y2, float zero)
{
    float y0 = zero;
    float a = (y2 - y1) / (x2 - x1);
    float b = y1 - a * x1; 
    float x0 = (y0 - b) / a; // x0 = -b/a
    if (x0 < x1 || x0 > x2) {
        ESP_LOGW("Zero crossing", "Invalid zero crossing index: %f", x0);
        return -1.;
    }
    if (x0 < 0.) {
        ESP_LOGW("Zero crossing", "Negative zero crossing index: %f", x0);
        return -1.;
    }
    return x0;
}


float ElecSignal::calcFrequency()
{
    constexpr float ERROR_VALUE = -1.0f;
    float lastTension = m_filteredDataBuffer.getData()->at(0);

    std::vector<float> fallingEgdeIndexes;
    std::vector<float> risingEgdeIndexes;


    //ESP_LOGI("Tension", "NB_FULL_PERIODS=%i", NB_FULL_PERIODS);

    fallingEgdeIndexes.reserve(NB_FULL_PERIODS);
    risingEgdeIndexes.reserve(NB_FULL_PERIODS);


    for (uint16_t i = 1; i < BUFFER_SIZE; i++) {
        float currentTension = m_filteredDataBuffer.getData()->at(i);
        
        // Rising edge detection
        if (lastTension < 0 && currentTension >= 0) {
            float zeroCrossingIndex = calcZeroCrossingIndex(i - 1, lastTension, i, currentTension);
            risingEgdeIndexes.push_back(zeroCrossingIndex);
        }

        // Falling edge detection
        if (lastTension > 0 && currentTension <= 0) {
            float zeroCrossingIndex = calcZeroCrossingIndex(i - 1, lastTension, i, currentTension);
            fallingEgdeIndexes.push_back(zeroCrossingIndex);
        }

        if (risingEgdeIndexes.size() >= NB_FULL_PERIODS && fallingEgdeIndexes.size() >= NB_FULL_PERIODS) {
            break;
        }

        lastTension = currentTension;
    }

    if (risingEgdeIndexes.size() < NB_FULL_PERIODS || fallingEgdeIndexes.size() < NB_FULL_PERIODS) {
        ESP_LOGE("Tension", "Error on zero crossing detection: %d-%d", risingEgdeIndexes.size(), fallingEgdeIndexes.size());
        return ERROR_VALUE;
    }

    std::string risingEgdeIndexesStr = "";
    for (float index : risingEgdeIndexes) {
        risingEgdeIndexesStr += std::to_string(index) + ",";
    }
    //ESP_LOGI("Tension", "Rising edge indexes: %s", risingEgdeIndexesStr.c_str());

    std::string fallingEgdeIndexesStr = "";
    for (float index : fallingEgdeIndexes) {
        fallingEgdeIndexesStr += std::to_string(index) + ",";
    }
    //ESP_LOGI("Tension", "Falling edge indexes: %s", fallingEgdeIndexesStr.c_str());

    // Calculate the average of the first and last zero crossing indexes
    float risingIndexesDelta = (risingEgdeIndexes[NB_FULL_PERIODS - 1] - risingEgdeIndexes[0]) / (NB_FULL_PERIODS - 1);
    float fallingIndexesDelta = (fallingEgdeIndexes[NB_FULL_PERIODS - 1] - fallingEgdeIndexes[0]) / (NB_FULL_PERIODS - 1);
    float meanIndexesDelta = (risingIndexesDelta + fallingIndexesDelta) / 2.;

    //ESP_LOGI("Tension", "Indexes delta - rising: %f - falling: %f - mean: %f", risingIndexesDelta, fallingIndexesDelta, meanIndexesDelta);

    float period = meanIndexesDelta / (ADC_FREQ / NB_CHANNELS); // in seconds
    float freq = 1. / period;   // in Hz
    //ESP_LOGI("Tension", "Period: %fs - Freq: %fHz", period, freq);

    // Robustness check on the frequency
    if (freq > MAX_AC_FREQ || freq < MIN_AC_FREQ) {
        ESP_LOGE("Tension", "Error: Frequency outside expected range: %fHz", freq);
        return ERROR_VALUE;
    }

    return period;
}