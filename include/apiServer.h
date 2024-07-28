#ifndef __APISERVER_H
#define __APISERVER_H

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include "lwip/ip4_addr.h"
#include <cJSON.h>
#include "def.h"

void wifi_init_sta(void);

#endif      // __APISERVER_H
