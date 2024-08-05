/*#ifndef __HTTPHANDLER_H
#define __HTTPHANDLER_H


#include "esp_http_server.h"

class HttpHandler
{
private:
    typedef esp_err_t (*HandlerFunction)(httpd_req_t *req);

    struct RequestData {
        httpd_req_t *req;
        bool new_request;
        HandlerFunction handler;
    };

    RequestData request_data;
    SemaphoreHandle_t request_mutex;
    TaskHandle_t task_handle;
    HandlerFunction handler_function;

    static void taskWrapper(void* param);
    void task();

public:
    HttpHandler(HandlerFunction handler);
    ~HttpHandler();
    void start();
    esp_err_t handleRequest(httpd_req_t *req);
};

#endif      // __HTTPHANDLER_H
*/