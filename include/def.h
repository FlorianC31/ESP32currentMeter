#ifndef __DEF_H
#define __DEF_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_system.h>
#include <esp_timer.h>
#include <esp_http_server.h>
#include <esp_adc/adc_continuous.h>
#include <esp_log.h>
#include <esp_netif.h>

#include <driver/gpio.h>
#include "nvs_flash.h"

#include <array>
#include <span>
#include <vector>
#include <string>
#include <atomic>
#include <cmath>

#ifndef UNUSED
#define UNUSED (void)
#endif

#define DEL_OBJ(x) if(x) {delete x;x=nullptr;}

// ADC configuration
#define NB_SAMPLES      128                                     // nb sample by main period
#define MAIN_FREQ       50                                      // hz
#define MAIN_PERIOD     (1000000 / MAIN_FREQ)                   // µs
#define NB_CURRENTS     8
#define NB_CHANNELS     (NB_CURRENTS + 2)
#define NB_SIGNALS      (NB_CHANNELS - 1)
#define SAMPLE_FREQ     (NB_SAMPLES * MAIN_FREQ)                // Hz
#define TIM_PERIOD      (1000000 / MAIN_FREQ / NB_SAMPLES)      // µs
#define TENSION_ID      (NB_CURRENTS + 0)
#define VREF_ID         (NB_CURRENTS + 1)
#define NB_QUEUE_CYCLES 4
#define NB_BUFF_CYCLES  2
#define BUFFER_SIZE     (NB_SAMPLES * NB_BUFF_CYCLES)
#define CHRN_FREQ_LIM   MAIN_FREQ                               // hz
#define NB_PERIODS_MEAN 10                                      // nb period for mean frequency calculation


// IRR filter configuration
#define FILTER_ORDER 2
#define MAX_HARM_FILTER 10
#define CUTOFF_FREQ (MAIN_FREQ * MAX_HARM_FILTER)


// Measure configuration
#define MEASURE_PACKET_PERIOD   (5 * 60)             // 5 minutes in seconds

// Network configuration
#define WIFI_SSID "Livebox-Florelie"
#define WIFI_PASS "r24hpkr2"
#define IP_ADDRESS "192.168.1.24"
#define NETMASK "255.255.255.0"
#define GATEWAY "192.168.1.1"
#define DNS_SERVER "8.8.8.8"              // Google's DNS server: 8.8.8.8

#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// Robustness protections
#define MIN_AC_FREQ   45.          // Hz
#define MAX_AC_FREQ   55.          // Hz

// Calibration coeff
#define CURRENT1_COEF_A     0.0279666256231008
#define CURRENT2_COEF_A     0.011393810439041
#define CURRENT3_COEF_A     0.011393810439041
#define CURRENT4_COEF_A     0.0165728151840597
#define CURRENT5_COEF_A     0.0379793681301368
#define CURRENT6_COEF_A     0.1
#define CURRENT7_COEF_A     0.1
#define CURRENT8_COEF_A     0.1
#define TENSION_COEF_A      0.178719296343853

#define CURRENT1_COEF_B     0.
#define CURRENT2_COEF_B     0.
#define CURRENT3_COEF_B     0.
#define CURRENT4_COEF_B     0.
#define CURRENT5_COEF_B     0.
#define CURRENT6_COEF_B     0.
#define CURRENT7_COEF_B     0.
#define CURRENT8_COEF_B     0.
#define TENSION_COEF_B      0.

#endif      // __DEF_H
