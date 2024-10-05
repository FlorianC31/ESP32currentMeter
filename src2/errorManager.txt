#include "errorManager.h"

#include <esp_log.h>

ErrorManager errorManager;

void ErrorManager::error(ErrorCode errorType, std::string className, std::string errorMsg)
{
    switch(errorType){
        default:
        case(GENERIC_ERROR) :
            ESP_LOGI(className.c_str(), "Generic Error : %s", errorMsg.c_str());
            break;
        case(INIT_ERROR) :
            ESP_LOGI(className.c_str(), "Initialization Error : %s", errorMsg.c_str());
            break;
        case(IMPOSSIBLE_VALUE_ERROR) :
            ESP_LOGI(className.c_str(), "Impossible Value Error : %s", errorMsg.c_str());
            break;
        case(PERFORMANCE_ERROR) :
            ESP_LOGI(className.c_str(), "Performance Error : %s", errorMsg.c_str());
            break;
        case(AC_FREQ_ERROR) :
            ESP_LOGI(className.c_str(), "AC Frequency Error : %s", errorMsg.c_str());
            break;
        case(TENSION_ERROR) :
            ESP_LOGI(className.c_str(), "Tension Error : %s", errorMsg.c_str());
            break;
    }
    
    Error error {errorType, className, errorMsg};
    m_errors.emplace_back(error);
}