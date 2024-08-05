/*#include "httpHandler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void HttpHandler::taskWrapper(void* param)
{
    static_cast<HttpHandler*>(param)->task();
}


void HttpHandler::task()
{
    while (1) {
        if (xSemaphoreTake(request_mutex, portMAX_DELAY) == pdTRUE) {
            if (request_data.new_request) {
                // Process the request here
                // This code is guaranteed to run on core 1
                if (handler_function) {
                    handler_function(request_data.req);
                }
                request_data.new_request = false;
            }
            xSemaphoreGive(request_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void HttpHandler::start()
{
    xTaskCreatePinnedToCore(taskWrapper, "HTTP Handler Task", 4096, this, 5, &task_handle, 1);
}


esp_err_t HttpHandler::handleRequest(httpd_req_t *req)
{
    if (xSemaphoreTake(request_mutex, portMAX_DELAY) == pdTRUE) {
        request_data.req = req;
        request_data.new_request = true;
        xSemaphoreGive(request_mutex);
    }

    // Wait for the core 1 task to process the request
    while (1) {
        if (xSemaphoreTake(request_mutex, portMAX_DELAY) == pdTRUE) {
            if (!request_data.new_request) {
                xSemaphoreGive(request_mutex);
                break;
            }
            xSemaphoreGive(request_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return ESP_OK;
}


HttpHandler::~HttpHandler()
{
    if (task_handle) {
        vTaskDelete(task_handle);
    }
    vSemaphoreDelete(request_mutex);
}*/
