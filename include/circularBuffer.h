#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "def.h"
#include "cJSON.h"


/**
 * @brief A thread-safe circular buffer implementation for ESP32
 * 
 * @tparam T The type of elements stored in the buffer
 * @tparam Size The fixed size of the buffer
 * 
 * This class implements a circular buffer that can be safely accessed
 * from multiple threads using FreeRTOS synchronization primitives.
 * Includes functionality to convert buffer contents to JSON format.
 */
class CircularBuffer {
public:
    CircularBuffer(std::string name);
    ~CircularBuffer();

    bool addData(const std::array<uint16_t, NB_CHANNELS> &data);
    bool calcOrderBuffer();
    std::string getData();

private:
    std::string m_name;
    std::array<std::array<int, BUFFER_SIZE>, NB_CHANNELS> m_orderedBuffer;
    std::array<std::array<uint16_t, NB_CHANNELS>, BUFFER_SIZE> m_buffer;
    u_int16_t m_index;
    SemaphoreHandle_t m_mutex;
};

#endif // CIRCULAR_BUFFER_H
