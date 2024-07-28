#ifndef __ERRORMANAGER_H
#define __ERRORMANAGER_H

#include <list>
#include <string>

extern ErrorManager errorManager;


typedef enum {
    GENERIC_ERROR = 0,
    INIT_ERROR,
    IMPOSSIBLE_VALUE_ERROR,
    PERFORMANCE_ERROR,
    AC_FREQ_ERROR,
    TENSION_ERROR
} ErrorCode;

class ErrorManager
{
public:
    ErrorManager() {};
    ~ErrorManager() {}

    void error(ErrorCode errorType, std::string className, std::string errorMsg);
    std::string getJson();

private:
    typedef struct {
        ErrorCode errorType;
        std::string className;
        std::string errorMsg;
    } Error;

    std::list<Error> m_errors;
};

#endif      // __ERRORMANAGER_H
