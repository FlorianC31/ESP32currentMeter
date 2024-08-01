#include "apiServer.h"
#include "adc.h"
#include "measure.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

// Variable volatile utilisée pour déclencher des actions en réponse aux requêtes HTTP
static volatile bool actionFlag = false;

/**
 * @brief Handler pour obtenir les données ADC via une requête HTTP GET.
 * 
 * Cette fonction lit les valeurs ADC et les retourne en tant que réponse JSON.
 * @param req La requête HTTP reçue.
 * @return esp_err_t ESP_OK si la requête est traitée avec succès.
 */
static esp_err_t get_adc_data_handler(httpd_req_t *req) {
    
    //xSemaphoreTake(mutex, portMAX_DELAY); // Prendre le mutex pour accéder aux valeurs ADC en toute sécurité
    std::string json_string = measure.getData();
    //xSemaphoreGive(mutex); // Libérer le mutex
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string.c_str(), json_string.length());
    
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

    //xSemaphoreTake(mutex, portMAX_DELAY); // Prendre le mutex pour accéder aux valeurs ADC en toute sécurité
    
    
    std::string json_string = adcChrono.getGlobalStats();
    //xSemaphoreGive(mutex); // Libérer le mutex
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string.c_str(), json_string.length());

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
    xSemaphoreTake(mutex, portMAX_DELAY); // Prendre le mutex pour accéder à actionFlag en toute sécurité
    actionFlag = true;
    xSemaphoreGive(mutex); // Libérer le mutex
    
    httpd_resp_send(req, "Action triggered", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Démarre le serveur web HTTP.
 * 
 * Cette fonction configure et démarre le serveur web, et enregistre les handlers pour les URI.
 * @return httpd_handle_t Handle du serveur web, ou NULL si le démarrage échoue.
 */
static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    
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
        
        httpd_uri_t uri_post = {
            .uri      = "/api/action",
            .method   = HTTP_POST,
            .handler  = trigger_action_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);
    }
    
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
        start_webserver();
    }
}

/**
 * @brief Initialise la connexion WiFi en mode station.
 * 
 * Cette fonction configure l'interface WiFi en mode station avec une adresse IP statique et démarre la connexion WiFi.
 */
void wifi_init_sta(void) {
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
}
