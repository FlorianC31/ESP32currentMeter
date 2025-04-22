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
#include <map>
#include <string>
#include <atomic>
#include <cmath>

#ifndef UNUSED
#define UNUSED (void)
#endif

#define DEL_OBJ(x) if(x) {delete x;x=nullptr;}

// ADC configuration
#define MAIN_FREQ       50                                      // hz
#define NB_SAMPLES      100                                     // samples number by main period
#define MAIN_PERIOD     (1000000 / MAIN_FREQ)                   // µs
#define NB_CURRENTS     8
#define NB_CHANNELS     (NB_CURRENTS + 2)
#define NB_SIGNALS      (NB_CHANNELS - 1)
#define SAMPLE_FREQ     (NB_SAMPLES * MAIN_FREQ)                // Hz
#define TIM_PERIOD      (1000000 / MAIN_FREQ / NB_SAMPLES)      // µs
#define TENSION_ID      0                                       // first channel
#define VREF_ID         NB_SIGNALS  // Vref signal ID           // last channel
#define BUFFER_SIZE     1024
#define NB_FULL_PERIODS int(BUFFER_SIZE/NB_SAMPLES)             // number of full periods in the buffer
#define QUEUE_SIZE      (NB_SAMPLES * 2)                        // 2 periods of samples
#define CHRN_FREQ_LIM   MAIN_FREQ                               // hz

#define TARGET_ADC_FREQ         (MAIN_FREQ * NB_SAMPLES * NB_CHANNELS)                                          // Hz
#define F_DIGI_CON              5000000.                                                                        // Hz, see soc_caps.h line 115
#define ADC_FREQ_DIVIDER        (F_DIGI_CON / 2. / TARGET_ADC_FREQ)                                             // float ADC frequency divider
#define ADC_FREQ_DIVIDER_INT    std::round(ADC_FREQ_DIVIDER)                                                    // rounded ADC frequency divider
#define ADC_FREQ                static_cast<u_int32_t>(std::round(F_DIGI_CON / 2. / ADC_FREQ_DIVIDER_INT))      // Hz

// IRR filter configuration
#define FILTER_ORDER    2
#define CUTOFF_FREQ     250             // Hz


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
#define CURRENT1_COEF       0.0279666256231008
#define CURRENT2_COEF       0.011393810439041
#define CURRENT3_COEF       0.011393810439041
#define CURRENT4_COEF       0.0165728151840597
#define CURRENT5_COEF       0.0379793681301368
#define CURRENT6_COEF       0.1
#define CURRENT7_COEF       0.1
#define CURRENT8_COEF       0.1
#define TENSION_COEF        0.178719296343853


#endif      // __DEF_H
