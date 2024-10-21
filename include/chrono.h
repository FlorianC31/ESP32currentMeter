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

    struct Data{
        float max = 0.;
        float min = 1000000.;
        float mean = 0.;
        int nbOverDeadline = 0;
        std::atomic<float> globalMax = 0.;
        std::atomic<float> globalMin = 1000000.;
        std::atomic<float> globalMean = 0.;
        std::atomic<float> globalNbOverDeadline = 0;
        float limit = 0.;
    };

    void print();
    void init();
    void calcGlobal();
    void calc(Data* data, float value);

    std::string m_name;

    int m_limit;
    bool m_debug;
    int m_printFreq;
    int m_iter = 0;
    double m_totalIter = 0.;
    int m_startTime;
    int m_lastStartTime;
    float m_curentFreq = 0.;
    
    Data m_freq;
    Data m_duration;
    Data m_cpuUsage;

};

#endif  // __CHRONO_H
