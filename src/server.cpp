#include "server.h"
#include "def.h"
#include <math.h>
#include "globalVar.h"
#include "ntp.h"


static esp_err_t get_adc_buffer_handler(httpd_req_t *req) {

    // header Content-Type configuration
    httpd_resp_set_type(req, "application/octet-stream");
    
    // Sending binary data
    esp_err_t res = httpd_resp_send(req, 
        reinterpret_cast<const char*>(adcBuffer.getBinData()->data()),
        sizeof(float) * NB_SIGNALS * BUFFER_SIZE
    );
    
    return res;
}

static esp_err_t get_adc_data_handler(httpd_req_t *req) {
    std::string json_string = measure.getJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string.c_str(), json_string.length());
    return ESP_OK;
}

static esp_err_t get_adc_chrono_handler(httpd_req_t *req) {
    bool first = true;
    std::string jsonStr = "{";
    for (Chrono* &chrono : chronoList) {
        if (first) {
            first = false;
        }
        else {
            jsonStr += ",";
        }
        jsonStr += "\"" + chrono->getName() + "\":" + chrono->getGlobalStats();
    }
    jsonStr += "}";
        
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonStr.c_str(), jsonStr.length());
    return ESP_OK;
}

static esp_err_t get_memory_handler(httpd_req_t *req) {
    ESP_LOGI("HTTP", "Handling request for URI: %s on socket: %d", 
        req->uri, httpd_req_to_sockfd(req));

    cJSON *json = cJSON_CreateObject();

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);

    cJSON_AddNumberToObject(json, "Total heap size (kB)", float(info.total_free_bytes + info.total_allocated_bytes) / 1000.);
    cJSON_AddNumberToObject(json, "Free heap size (kB)", float(info.total_free_bytes) / 1000.);
    cJSON_AddNumberToObject(json, "Allocated heap size (kB)", float(info.total_allocated_bytes) / 1000.);
    cJSON_AddNumberToObject(json, "Minimum free heap size (kB)", float(info.minimum_free_bytes) / 1000.);
    cJSON_AddNumberToObject(json, "Size of adcBuffer (kB)", float(sizeof(adcBuffer)) / 1000.);
    cJSON_AddNumberToObject(json, "Size of adcDataQueue (kB)", float(sizeof(adcDataQueue)) / 1000.);

    char* jsonStr = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonStr, strlen(jsonStr));

    free(jsonStr);
    cJSON_Delete(json); 

    return ESP_OK;
}

static esp_err_t get_time_handler(httpd_req_t *req) {
    time_t now = get_timestamp();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buffer[100];

    // Formater la chaîne dans le tampon
    snprintf(buffer, sizeof(buffer), "{\"Current time\": \"%02d:%02d:%02d %02d/%02d/%04d \"}",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    std::string json_string(buffer);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string.c_str(), json_string.length());


    return ESP_OK;
}

    
static esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, "", 0);  // Send empty response
    return ESP_OK;
}


/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.keep_alive_enable = false;
    config.max_open_sockets = 3;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 1;  // 1 second timeout
    config.send_wait_timeout = 1;

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    httpd_uri_t uri_favicon = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = favicon_get_handler,
        .user_ctx = NULL
    };

    httpd_uri_t uri_getAdcData = {
        .uri      = "/api/adc/data",
        .method   = HTTP_GET,
        .handler  = get_adc_data_handler,
        .user_ctx = NULL
    };
    httpd_uri_t uri_getAdcChrono = {
        .uri      = "/api/adc/chrono",
        .method   = HTTP_GET,
        .handler  = get_adc_chrono_handler,
        .user_ctx = NULL
    };
    httpd_uri_t uri_getAdcBuffer = {
        .uri      = "/api/adc/buffer",
        .method   = HTTP_GET,
        .handler  = get_adc_buffer_handler,
        .user_ctx = NULL
    };
    httpd_uri_t uri_getMemory = {
        .uri      = "/api/memory",
        .method   = HTTP_GET,
        .handler  = get_memory_handler,
        .user_ctx = NULL
    };        
    httpd_uri_t uri_getTime = {
        .uri      = "/api/time",
        .method   = HTTP_GET,
        .handler  = get_time_handler,
        .user_ctx = NULL
    };

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_favicon);
        httpd_register_uri_handler(server, &uri_getAdcBuffer);
        httpd_register_uri_handler(server, &uri_getMemory);
        httpd_register_uri_handler(server, &uri_getTime);
        httpd_register_uri_handler(server, &uri_getAdcChrono);
        httpd_register_uri_handler(server, &uri_getAdcData);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}

