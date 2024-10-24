#ifndef __DEF_H
#define __DEF_H

#ifndef UNUSED
#define UNUSED (void)
#endif

#define DEL_OBJ(x) if(x) {delete x;x=nullptr;}

// ADC configuration
#define NB_SAMPLES      128
#define ANALYZED_PERIOD 20.480                                  // ms (A little bit more than 1/50Hz = 20ms)
#define NB_CURRENTS     5
#define NB_CHANNELS     (NB_CURRENTS + 2)
#define SAMPLE_RATE     (1000 / ANALYZED_PERIOD * NB_SAMPLES)
#define TIM_PERIOD      (ANALYZED_PERIOD * 1000 / NB_SAMPLES)     // µs
#define TENSION_ID      (NB_CURRENTS + 0)
#define VREF_ID         (NB_CURRENTS + 1)

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

// 5 currents channels and 1 tension (y = A . x + B)
const float CALIB_A_COEFFS[] = {0.0387, 0.0168, 0.0162, 0.0233, 0.0538, 0.412572};       // 5 currents channels and 1 tension (y = A . x + B)
const float CALIB_B_COEFFS[] = {0., 0., 0., 0., 0., 0.}; //{0.014, -0.006, -0.054, -0.0515, 0.0395, 0.2065};



#endif      // __DEF_H
