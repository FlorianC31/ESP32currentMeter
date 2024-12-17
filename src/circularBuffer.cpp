#include "circularBuffer.h"
#include "chrono.h"
#include "globalVar.h"


/**
 * @brief Construct a new Circular Buffer object
 */
CircularBuffer::CircularBuffer() :
    m_index(0)
{
    m_writeBuffer = &m_buffer1;
    m_readBuffer = &m_buffer2;
}

/**
 * @brief Destroy the Circular Buffer object
 */
CircularBuffer::~CircularBuffer()
{}

/**
 * @brief Write a single element to the buffer
 * 
 * @param value The value to write
 * @return true if write was successful
 */
bool CircularBuffer::addData(const std::array<float, NB_SIGNALS> &data)
{
    auto* currentWriteBuffer = m_writeBuffer.load();
    for (uint8_t channelId = 0; channelId < NB_SIGNALS; channelId++) {
        currentWriteBuffer->at(channelId)[m_index] = data[channelId];
    }
    m_index = (m_index + 1) % BUFFER_SIZE;
    return true;
}


/**
 * @brief Convert the entire buffer to JSON object
 * 
 * @param channelNames Optional array of channel names
 * @return std::string JSON representation of the buffer
 */
std::string CircularBuffer::getData()
{
    /*bufferTotalChrono.startCycle();
    cJSON* json = cJSON_CreateObject();
    std::array<cJSON*, NB_CHANNELS> jsonArrays;

    
    ESP_LOGI("TAG", "  Buffer1 -> %p", &m_buffer1);
    ESP_LOGI("TAG", "  Buffer1 -> %p", &m_buffer2);

    ESP_LOGI("TAG", "Avant échange :");
    ESP_LOGI("TAG", "  WriteBuffer -> %p", (void*)m_writeBuffer.load());
    ESP_LOGI("TAG", "  ReadBuffer  -> %p", (void*)m_readBuffer.load());

    m_readBuffer.store(std::atomic_exchange(&m_writeBuffer, m_readBuffer.load()));

    ESP_LOGI("TAG", "Après échange :");
    ESP_LOGI("TAG", "  WriteBuffer -> %p", (void*)m_writeBuffer.load());
    ESP_LOGI("TAG", "  ReadBuffer  -> %p", (void*)m_readBuffer.load());

    cJSON_AddNumberToObject(json, "bufferIndex", m_index);

    
    ESP_LOGI("TAG", "Flag0");
    
    auto* currentReadBuffer = m_readBuffer.load(); 
    for (uint8_t channelId = 0; channelId < NB_CHANNELS; channelId++) {
        jsonArrays[channelId] = cJSON_CreateIntArray(currentReadBuffer->at(channelId).data(), currentReadBuffer->at(channelId).size());
    }

    ESP_LOGI("TAG", "Flag1");

    for (uint8_t channelId = 0; channelId < NB_CHANNELS; channelId++) {
        std::string arrayName;
        if (channelId < TENSION_ID) {
            arrayName = "Current" + std::to_string(channelId + 1);
        }
        else if (channelId == TENSION_ID) {
            arrayName = "Tension";
        }
        else if (channelId == VREF_ID) {
            arrayName = "Vref";
        }
        else  {
            arrayName = "Error";
        }

        cJSON_AddItemToObject(json, arrayName.c_str(), jsonArrays[channelId]);
    }
    

    std::unique_ptr<char, decltype(&free)> jsonStr(cJSON_Print(json), free);
    if (!jsonStr) {
        return "{}"; // Return empty JSON on string generation failure
    }

    return std::string(jsonStr.get());*/
    return "";
}


std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS>* CircularBuffer::getBinData()
{
    m_readBuffer.store(std::atomic_exchange(&m_writeBuffer, m_readBuffer.load()));
    return m_readBuffer.load();
}
