#ifndef __CHRONO_H
#define __CHRONO_H

#include <string>


class Chrono
{
public:
    Chrono(std::string name, int printFreq = 1000);
    ~Chrono() {};
    void startCycle();
    void endCycle();

private:
    void print();
    void init();

    std::string m_name;
    int m_printFreq;
    int m_iter = 0;
    int64_t m_maxTime = 0;
    int64_t m_minTime = 1000000;
    int64_t m_meanTime = 0;
    int64_t m_startTime;

};

#endif  // __CHRONO_H
