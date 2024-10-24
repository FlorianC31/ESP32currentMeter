#include "circularBuffer.h"
#include "chrono.h"
#include "globalVar.h"

/**
 * @brief Construct a new Circular Buffer object
 */
CircularBuffer::CircularBuffer(std::string name) :
    m_name(name),
    m_index(0)
{
    m_mutex = xSemaphoreCreateMutex();
}

/**
 * @brief Destroy the Circular Buffer object
 */
CircularBuffer::~CircularBuffer()
{
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
    }
}

/**
 * @brief Write a single element to the buffer
 * 
 * @param value The value to write
 * @return true if write was successful
 */
bool CircularBuffer::addData(const std::array<uint16_t, NB_CHANNELS> &data)
{
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t channelId = 0; channelId < NB_CHANNELS ; channelId++) {
            m_buffer[channelId][m_index] = data[channelId];
        }
        m_index = (m_index + 1) % BUFFER_SIZE;
        xSemaphoreGive(m_mutex);
        return true;
    }
    return false;
}


/**
 * @brief Convert the entire buffer to JSON object
 * 
 * @param channelNames Optional array of channel names
 * @return std::string JSON representation of the buffer
 */
std::string CircularBuffer::getData()
{
    bufferTotalChrono.startCycle();
    cJSON* json = cJSON_CreateObject();
    std::array<cJSON*, NB_CHANNELS> jsonArrays;

    bufferMutexChrono.startCycle();
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        cJSON_AddNumberToObject(json, "bufferIndex", m_index);

        for (uint8_t channelId = 0; channelId < NB_CHANNELS; channelId++) {
            jsonArrays[channelId] = cJSON_CreateIntArray(m_buffer[channelId].data(), m_buffer[channelId].size());
        }
        bufferMutexChrono.endCycle();
        xSemaphoreGive(m_mutex);

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
    }

    std::string bufferString = cJSON_Print(json);
    cJSON_Delete(json);

    bufferTotalChrono.endCycle();


    return bufferString;
}

