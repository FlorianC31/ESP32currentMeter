#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <esp_heap_caps.h>

#include "globalVar.h"

#include "wifi.h"
#include "adc.h"
//#include "measure.h"
#include "chrono.h"
#include "ntp.h"


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



// Variable volatile utilisée pour déclencher des actions en réponse aux requêtes HTTP
static volatile bool actionFlag = false;

void log_socket_states() {
    ESP_LOGI("HTTP", "Socket states:");
    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(i, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            ESP_LOGI("HTTP", "Socket %d is active, error state: %d", i, error);
        }
    }
}


httpd_handle_t server = NULL;

void cleanup_networking() {
    if (server) {
        ESP_LOGI("HTTP", "Starting cleanup");
        log_socket_states();  // Log initial socket states
        
        // Log initial memory state
        multi_heap_info_t info_start;
        heap_caps_get_info(&info_start, MALLOC_CAP_DEFAULT);
        ESP_LOGI("HTTP", "Cleanup start - Memory: %.3f kB", 
                 info_start.total_allocated_bytes / 1000.0f);

        // Close all active sessions
        for (int sock_fd = 0; sock_fd < CONFIG_LWIP_MAX_SOCKETS; sock_fd++) {
            if (httpd_sess_get_ctx(server, sock_fd)) {
                ESP_LOGI("HTTP", "Closing session for socket %d", sock_fd);
                httpd_sess_trigger_close(server, sock_fd);
                // Add small delay to allow closure
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        // Stop the server
        ESP_LOGI("HTTP", "Stopping HTTP server");
        //httpd_stop(server);
        server = NULL;

        // Force socket cleanup at TCP/IP level
        ESP_LOGI("HTTP", "Forcing socket cleanup");
        for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
            close(i);
        }

        // Try to force memory cleanup
        ESP_LOGI("HTTP", "Attempting memory cleanup");
        for (int i = 0; i < 3; i++) {  // Try multiple times
            size_t free_size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            if (free_size > 1024) {
                void* temp = heap_caps_malloc(free_size - 1024, MALLOC_CAP_8BIT);
                if (temp) {
                    heap_caps_free(temp);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));  // Give some time for cleanup
        }

        log_socket_states();  // Log final socket states

        // Log final memory state
        multi_heap_info_t info_end;
        heap_caps_get_info(&info_end, MALLOC_CAP_DEFAULT);
        ESP_LOGI("HTTP", "Cleanup complete - Memory: %.3f kB (Delta: %.3f kB)", 
                 info_end.total_allocated_bytes / 1000.0f,
                 (info_end.total_allocated_bytes - info_start.total_allocated_bytes) / 1000.0f);
    }
}

/**
 * @brief Handler pour obtenir les données ADC via une requête HTTP GET.
 * 
 * Cette fonction lit les valeurs ADC et les retourne en tant que réponse JSON.
 * @param req La requête HTTP reçue.
 * @return esp_err_t ESP_OK si la requête est traitée avec succès.
 */
static esp_err_t get_adc_data_handler(httpd_req_t *req) {
    //std::string json_string = measure.getJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, " ", 1);
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_stop(server);
    return ESP_OK;
}

/**
 * @brief Handler pour obtenir les statistiques de Chrono via une requête HTTP GET.
 * 
 * Cette fonction retourne les statistiques de Chrono sous forme de réponse JSON.
 * @param req La requête HTTP reçue.
 * @return esp_err_t ESP_OK si la requête est traitée avec succès.
 */
static esp_err_t get_adc_chrono_handler(httpd_req_t *req) {
    ESP_LOGI("HTTP", "Handling request for URI: %s on socket: %d", 
            req->uri, httpd_req_to_sockfd(req));

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
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, jsonStr.c_str(), jsonStr.length());
    httpd_sess_trigger_close(server, httpd_req_to_sockfd(req));

    //cleanup_networking();
    //httpd_stop(server);
    log_socket_states();

    return ESP_OK;
}

// TODO : try this : https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_http_server.html#application-example



static esp_err_t get_adc_buffer_handler(httpd_req_t *req) {
    ESP_LOGI("HTTP", "Handling request for URI: %s on socket: %d", 
        req->uri, httpd_req_to_sockfd(req));


    std::string json_string = adcBuffer.getData();
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string.c_str(), json_string.length());
    httpd_resp_set_hdr(req, "Connection", "close");
    
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
    httpd_resp_set_hdr(req, "Connection", "close");

    free(jsonStr);
    cJSON_Delete(json); 

    return ESP_OK;
}

static esp_err_t get_time_handler(httpd_req_t *req) {
    ESP_LOGI("HTTP", "Handling request for URI: %s on socket: %d", 
        req->uri, httpd_req_to_sockfd(req));

    cJSON *json = cJSON_CreateObject();

    time_t now = get_timestamp();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buffer[100];

    // Formater la chaîne dans le tampon
    snprintf(buffer, sizeof(buffer), "Current time: %02d:%02d:%02d %02d/%02d/%04d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    cJSON_AddStringToObject(json, "Datetime", buffer);

    std::string json_string = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string.c_str(), json_string.length());
    httpd_resp_set_hdr(req, "Connection", "close");

    cJSON_Delete(json);

    return ESP_OK;
}

/**
 * @brief Handler pour déclencher une action via une requête HTTP POST.
 * 
 * Cette fonction met à jour un flag pour déclencher une action.
 * @param req La requête HTTP reçue.
 * @return esp_err_t ESP_OK si la requête est traitée avec succès.
 */
static esp_err_t trigger_action_handler(httpd_req_t *req) {
    //xSemaphoreTake(mutex, portMAX_DELAY); // Prendre le mutex pour accéder à actionFlag en toute sécurité
    actionFlag = true;
    //xSemaphoreGive(mutex); // Libérer le mutex
    
    httpd_resp_send(req, "Action triggered", HTTPD_RESP_USE_STRLEN);
    httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t *req) {
    ESP_LOGI("HTTP", "Handling request for URI: %s on socket: %d", 
        req->uri, httpd_req_to_sockfd(req));
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, "", 0);  // Send empty response
    return ESP_OK;
}

int http_conn_handler(httpd_handle_t hd, int sockfd) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGI("HTTP", "Connection established. Socket fd: %d, Allocated heap: %.3f kB", 
             sockfd, info.total_allocated_bytes / 1000.0f);
    log_socket_states();
    return ESP_OK;
}

void http_disconn_handler(httpd_handle_t hd, int sockfd) {
    ESP_LOGI("HTTP", "Connection closing started for socket: %d", sockfd);
    log_socket_states();  // Log state before socket closes
    
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    if (getpeername(sockfd, (struct sockaddr*)&addr, &addr_size) == 0) {
        ESP_LOGI("HTTP", "Socket %d was connected to port %d", 
                 sockfd, 
                 ntohs(addr.sin_port));
    }
    
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGI("HTTP", "Memory before socket %d close: %.3f kB", 
             sockfd, info.total_allocated_bytes / 1000.0f);
    
    close(sockfd);
    
    log_socket_states();  // Log state after socket closes
    
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    ESP_LOGI("HTTP", "Memory after socket %d close: %.3f kB", 
             sockfd, info.total_allocated_bytes / 1000.0f);
}


/**
 * @brief Démarre le serveur web HTTP.
 * 
 * Cette fonction configure et démarre le serveur web, et enregistre les handlers pour les URI.
 * @return httpd_handle_t Handle du serveur web, ou NULL si le démarrage échoue.
 */
httpd_handle_t start_webserver(void) {

    static const char* TAG = "start_webserver";

    ESP_LOGW(TAG, "Start");


    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.keep_alive_enable = false;
    config.max_open_sockets = 3;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 1;  // 1 second timeout
    config.send_wait_timeout = 1;
    config.open_fn = http_conn_handler;
    config.close_fn = http_disconn_handler;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_getAdcData = {
            .uri      = "/api/adc/data",
            .method   = HTTP_GET,
            .handler  = get_adc_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_getAdcData);

        httpd_uri_t uri_getAdcChrono = {
            .uri      = "/api/adc/chrono",
            .method   = HTTP_GET,
            .handler  = get_adc_chrono_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_getAdcChrono);

        httpd_uri_t uri_getAdcBuffer = {
            .uri      = "/api/adc/buffer",
            .method   = HTTP_GET,
            .handler  = get_adc_buffer_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_getAdcBuffer);
        
        httpd_uri_t uri_getMemory = {
            .uri      = "/api/memory",
            .method   = HTTP_GET,
            .handler  = get_memory_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_getMemory);
        
        httpd_uri_t uri_getTime = {
            .uri      = "/api/time",
            .method   = HTTP_GET,
            .handler  = get_time_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_getTime);

        httpd_uri_t uri_post = {
            .uri      = "/api/action",
            .method   = HTTP_POST,
            .handler  = trigger_action_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);

        httpd_uri_t uri_favicon = {
            .uri      = "/favicon.ico",
            .method   = HTTP_GET,
            .handler  = favicon_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_favicon);
    }
    
    ESP_LOGW(TAG, "Web Server correctly initialized");

    return server;
}

/**
 * @brief Handler pour les événements WiFi.
 * 
 * Cette fonction gère les événements WiFi tels que la connexion et la déconnexion, et démarre le serveur web lorsqu'une adresse IP est obtenue.
 * @param arg Arguments passés à l'handler (non utilisés).
 * @param event_base Base de l'événement.
 * @param event_id Identifiant de l'événement.
 * @param event_data Données de l'événement.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ntpSyncTime();
    }
}

/**
 * @brief Initialise la connexion WiFi en mode station.
 * 
 * Cette fonction configure l'interface WiFi en mode station avec une adresse IP statique et démarre la connexion WiFi.
 */
void wifi_init_sta(void) {
    static const char* TAG = "wifi_init_sta";
    ESP_LOGW(TAG, "Wifi initializing");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Crée l'interface WiFi station par défaut
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    
    // Désactive le client DHCP
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));
    
    // Configure l'adresse IP statique
    esp_netif_ip_info_t ip_info = {};
    
    // Convertit les adresses IP sous forme de chaîne en format approprié
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(IP_ADDRESS, &ip_info.ip));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(NETMASK, &ip_info.netmask));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(GATEWAY, &ip_info.gw));
    
    // Configure les informations IP
    ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));

    // Add DNS configuration
    esp_netif_dns_info_t dns_info = {};
    esp_ip4_addr_t dns_server;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(DNS_SERVER, &dns_server));  // Google's DNS server: 8.8.8.8
    dns_info.ip.u_addr.ip4 = dns_server;
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info));
   
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
   
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
   
    wifi_config_t wifi_config = {};  // Initialisation par défaut de tous les membres
    wifi_config.sta.ssid[0] = '\0';  // Initialise comme chaîne vide
    wifi_config.sta.password[0] = '\0';  // Initialise comme chaîne vide
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.bssid_set = false;
    wifi_config.sta.channel = 0;
    wifi_config.sta.listen_interval = 0;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGW(TAG, "Wifi correctly initialized");
}
