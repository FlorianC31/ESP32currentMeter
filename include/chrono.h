#ifndef __CHRONO_H
#define __CHRONO_H

#include <string>
#include <cJSON.h>
#include <atomic>


class Chrono
{
public:
    Chrono(std::string name, int limit, int printFreq = 1000, bool debug = false);
    ~Chrono();
    void startCycle();
    void endCycle();
    std::string getGlobalStats();

private:
    void print();
    void init();
    void calcGlobal();

    std::string m_name;
    int m_limit;
    bool m_debug;
    int m_printFreq;
    int m_iter = 0;
    int m_maxTime = 0;
    int m_minTime = 1000000;
    int m_meanTime = 0;
    int m_startTime;

    std::atomic<int> m_totalIter = 0;
    std::atomic<int> m_totalMaxTime = 0;
    std::atomic<int> m_totalMinTime = 1000000;
    std::atomic<int> m_totalMeanTime = 0;
    std::atomic<int> m_nbOverLimit = 0;

    cJSON* m_globalStats;
};

#endif  // __CHRONO_H
