#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "secrets.h" // Importiert Passwörter

// --- KONFIGURATION ---

const long SLOW_DATA_UPDATE_SEC = 900; 


// Intervalle
const int INTERVAL_FAST   = 5;  
const int INTERVAL_NORMAL = 15; 
const int INTERVAL_NIGHT  = 60; 

// PINS (Seeed XIAO / reTerminal E1001)
const int BAT_ADC_PIN = 1;    
const int BAT_EN_PIN  = 21;   
const int KEY0 = 3; 
const int KEY1 = 4; 
const int KEY2 = 5; 

// E-Ink Pins
#define EPD_SCK_PIN  7
#define EPD_MOSI_PIN 9
#define EPD_CS_PIN   10
#define EPD_DC_PIN   11
#define EPD_RES_PIN  12
#define EPD_BUSY_PIN 13


#endif