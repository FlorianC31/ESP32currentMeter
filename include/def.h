#ifndef __DEF_H
#define __DEF_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_adc/adc_continuous.h>
#include <driver/gpio.h>
#include "nvs_flash.h"
#include <esp_log.h>

#include <array>
#include <vector>
#include <string>
#include <atomic>

#ifndef UNUSED
#define UNUSED (void)
#endif

#define DEL_OBJ(x) if(x) {delete x;x=nullptr;}

// ADC configuration
#define NB_SAMPLES      128
#define ANALYZED_PERIOD 20.480                                  // ms (A little bit more than 1/50Hz = 20ms)
#define MAIN_FREQ       (1000 / ANALYZED_PERIOD)                // hz
#define NB_CURRENTS     6
#define NB_CHANNELS     (NB_CURRENTS + 2)
#define SAMPLE_FREQ     (1000 * NB_SAMPLES / ANALYZED_PERIOD)   // Hz
#define TIM_PERIOD      (ANALYZED_PERIOD * 1000 / NB_SAMPLES)   // µs
#define TENSION_ID      (NB_CURRENTS + 0)
#define VREF_ID         (NB_CURRENTS + 1)
#define NB_QUEUE_CYCLES 4
#define NB_BUFF_CYCLES  2
#define BUFFER_SIZE     (NB_SAMPLES * NB_BUFF_CYCLES)
#define CHRN_FREQ_LIM   50                                      // hz


// Measure configuration
#define MEASURE_PACKET_PERIOD   (5 * 60)             // 5 minutes in seconds

// Network configuration
#define WIFI_SSID "Livebox-Florelie"
#define WIFI_PASS "r24hpkr2"
#define IP_ADDRESS "192.168.1.24"
#define NETMASK "255.255.255.0"
#define GATEWAY "192.168.1.1"
#define DNS_SERVER "8.8.8.8"

#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// Robustness protections
#define MIN_AC_FREQ   40.          // Hz
#define MAX_AC_FREQ   60.          // Hz

// 6 currents channels and 1 tension (y = A . x + B)
//const float CALIB_A_COEFFS[] = {0., 0., 0., 0., 0., 0.};       // 5 currents channels and 1 tension (y = A . x + B)
//const float CALIB_B_COEFFS[] = {0., 0., 0., 0., 0., 0.}; //{0.014, -0.006, -0.054, -0.0515, 0.0395, 0.2065};

#endif      // __DEF_H
