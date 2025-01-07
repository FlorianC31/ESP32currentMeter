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
    CircularBuffer();
    ~CircularBuffer();

    bool addData(const std::array<float, NB_SIGNALS> &data);
    std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS>* getData() {return m_readBuffer.load();}

private:
    u_int16_t m_index;
    
    std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS> m_buffer1;
    std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS> m_buffer2;
    std::atomic<std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS>*> m_readBuffer;
    std::atomic<std::array<std::array<float, BUFFER_SIZE>, NB_SIGNALS>*> m_writeBuffer;
};

#endif // CIRCULAR_BUFFER_H
