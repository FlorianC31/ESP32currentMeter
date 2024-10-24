#ifndef __CHRONO_H
#define __CHRONO_H

#include <string>
#include <cJSON.h>
#include <atomic>
#include <esp_log.h>


class Chrono
{
public:
    Chrono(std::string name, float limitCpu, int m_nbIgnored = 0, int printFreq = 0);
    ~Chrono();
    void startCycle();
    void endCycle();
    std::string getGlobalStats();
    std::string getName() {return m_name;}

private:

    struct Data{
        std::atomic<float> max = 0.;
        std::atomic<float> min = 1000000.;
        std::atomic<float> mean = 0.;
        std::atomic<int> nbOverDeadline = 0;
        bool freq = false;
        float limit = 0.;

        void add(float value, int nbIter) {
            if (value > max) {
                max = value;
            }
            else if (value < min) {
                min = value;
            }
            if ((freq && value < static_cast<float>(limit)) || (!freq && value > static_cast<float>(limit))){
                nbOverDeadline++;
            }
            mean = (mean * nbIter + value) / (nbIter + 1);
        }

        void init(float lim, bool isFreq = false) {
            max = 0.;
            min = 1000000.;
            mean = 0.;
            limit = lim;
            freq = isFreq;
        }

        cJSON* getJson(std::string unity, uint64_t nbIter) {
            cJSON* json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, ("min(" + unity + ")").c_str(), min);
            cJSON_AddNumberToObject(json, ("mean" + unity + ")").c_str(), mean);
            cJSON_AddNumberToObject(json, ("max(" + unity + ")").c_str(), max);
            if (freq) {
                cJSON_AddNumberToObject(json, std::string("nbUnder " + std::to_string(limit) + unity + "(%)").c_str(), (float)nbOverDeadline / nbIter * 100);
            }
            else {
                cJSON_AddNumberToObject(json, std::string("nbOver " + std::to_string(limit) + unity + "(%)").c_str(), (float)nbOverDeadline / nbIter * 100);
            }
            return json;
        }

        /*void print(std::string name, std::string unity) {
            ESP_LOGI(name.c_str(), "min: %f(%s) - max: %f(%s) - mean: %f(%s)", min, unity.c_str(), max, unity.c_str(), mean, unity.c_str());
        }*/
    };

    void print();
    void init();

    std::string m_name;

    float m_limitCpu;
    int m_nbIgnored = 0;
    int m_printFreq;

    uint64_t m_iter = 0;
    int m_startTime;
    int m_lastStartTime;
    
    Data m_freq;
    Data m_duration;
    Data m_cpuUsage;

};

#endif  // __CHRONO_H
