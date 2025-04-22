#include "alternatingBuffer.h"
#include "chrono.h"
#include "globalVar.h"


/**
 * @brief Construct a new Circular Buffer object
 */
AlternatingBuffer::AlternatingBuffer() :
    m_index(0),
    m_Lastindex(0),
    m_meanValue(0.),
    m_isBufferReadyForProcessing(false)
{
    m_writeBuffer = &m_buffer1;
    m_readBuffer = &m_buffer2;
}

/**
 * @brief Destroy the Circular Buffer object
 */
AlternatingBuffer::~AlternatingBuffer()
{}

/**
 * @brief Write a single element to the buffer
 * 
 * @param data The value to write
 */
void AlternatingBuffer::addData(const float &data)
{
    auto* currentWriteBuffer = m_writeBuffer.load();
    currentWriteBuffer->at(m_index) = data;
    m_meanValue += data;

    m_Lastindex = m_index;
    m_index++;

    if (m_index == BUFFER_SIZE) {
        //removeOffset();
        // swap buffers
        m_readBuffer.store(std::atomic_exchange(&m_writeBuffer, m_readBuffer.load()));
        m_isBufferReadyForProcessing = true;
        m_index = 0;
        m_meanValue = 0.;
    }
}

/**
 * @brief Check if the buffer is ready for processing
 * 
 * @return true if the buffer is ready for processing
 */
bool AlternatingBuffer::isBufferReadyForProcessing()
{
    if (m_isBufferReadyForProcessing) {
        // Reinitialize the flag before returning true
        m_isBufferReadyForProcessing = false;
        return true;
    }
    else {
        return false;
    }
}


/**
 * * @brief Remove the offset from the buffer
 * * @details This function calculates the mean value of the buffer and subtracts it from each element
 */
void AlternatingBuffer::removeOffset()
{
    float meanValue = m_meanValue / BUFFER_SIZE;
    for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
        m_writeBuffer.load()->at(i) -= meanValue;
    }
}