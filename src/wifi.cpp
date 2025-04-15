#include "def.h"
#include "wifi.h"

#include "globalVar.h"
#include "ntp.h"


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
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(DNS_SERVER, &dns_server));
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
