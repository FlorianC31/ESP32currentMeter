#ifndef __APISERVER_H
#define __APISERVER_H

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include "lwip/ip4_addr.h"
#include <cJSON.h>

// Network configuration
#define WIFI_SSID "Livebox-Florelie"
#define WIFI_PASS "r24hpkr2"
#define IP_ADDRESS "192.168.1.24"
#define NETMASK "255.255.255.0"
#define GATEWAY "192.168.1.1"

void wifi_init_sta(void);

#endif      // __APISERVER_H
