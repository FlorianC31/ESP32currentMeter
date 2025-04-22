#ifndef ALTERNATING_BUFFER_H
#define ALTERNATING_BUFFER_H

#include "def.h"
#include "cJSON.h"



class AlternatingBuffer {
public:
    AlternatingBuffer();
    ~AlternatingBuffer();

    void addData(const float &data);
    std::array<float, BUFFER_SIZE>* getData() const {return m_readBuffer.load();}
    float getLastValue() const {return m_readBuffer.load()->at(m_Lastindex);}
    void reset() {m_index = 0; m_readBuffer.store(&m_buffer2); m_writeBuffer.store(&m_buffer1);}
    bool isBufferReadyForProcessing();

private:
    void removeOffset();

private:
    u_int16_t m_index;
    u_int16_t m_Lastindex;
    float m_meanValue;

    std::atomic<bool> m_isBufferReadyForProcessing;
    
    std::array<float, BUFFER_SIZE> m_buffer1;
    std::array<float, BUFFER_SIZE> m_buffer2;
    std::atomic<std::array<float, BUFFER_SIZE>*> m_readBuffer;
    std::atomic<std::array<float, BUFFER_SIZE>*> m_writeBuffer;
};

#endif // ALTERNATING_BUFFER_H
