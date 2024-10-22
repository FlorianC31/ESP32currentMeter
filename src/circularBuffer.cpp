#include "circularBuffer.h"

/**
 * @brief Construct a new Circular Buffer object
 */
CircularBuffer::CircularBuffer() : 
    m_index(0)
{
    m_mutex = xSemaphoreCreateMutex();
}

/**
 * @brief Destroy the Circular Buffer object
 */
CircularBuffer::~CircularBuffer()
{
    DEL_OBJ(m_mutex);
}

/**
 * @brief Write a single element to the buffer
 * 
 * @param value The value to write
 * @return true if write was successful
 */
bool CircularBuffer::write(const std::array<uint16_t, NB_CHANNELS> &data)
{
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_buffer[m_index] = data;
        m_index = (m_index + 1) % BUFFER_SIZE;
        xSemaphoreGive(m_mutex);
        return true;
    }
    return false;
}


bool CircularBuffer::calcOrderBuffer()
{
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t channelId = 0; channelId < NB_CHANNELS; channelId++) {
            uint16_t i = 0;
            for (uint16_t j = m_index; j < BUFFER_SIZE; j++) {
                m_orderedBuffer[i][channelId] = static_cast<int>(m_buffer[j][channelId]);
                i++;
            }
            for (uint16_t j = 0; j < m_index; j++) {
                m_orderedBuffer[i][channelId] = static_cast<int>(m_buffer[j][channelId]);
                i++;
            }
        }
        xSemaphoreGive(m_mutex);
        return true;
    }
    else {
        return false;
    }
}

/**
 * @brief Convert the entire buffer to JSON object
 * 
 * @param channelNames Optional array of channel names
 * @return std::string JSON representation of the buffer
 */
cJSON* CircularBuffer::getJson()
{
    cJSON* json = cJSON_CreateObject();

    if (calcOrderBuffer()) {
        for (uint8_t channelId = 0; channelId < NB_CHANNELS; channelId++) {
            cJSON* array = cJSON_CreateIntArray(m_orderedBuffer[channelId].data(), m_orderedBuffer[channelId].size());
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

            cJSON_AddItemToObject(json, arrayName.c_str(), array);
        }
    }

    return json;
}

