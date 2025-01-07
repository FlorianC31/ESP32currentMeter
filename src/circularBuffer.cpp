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
    m_index++;

    if (m_index == BUFFER_SIZE) {
        // swap buffers
        m_readBuffer.store(std::atomic_exchange(&m_writeBuffer, m_readBuffer.load()));

        m_index = 0;
        return true;
    }
    return false;
}