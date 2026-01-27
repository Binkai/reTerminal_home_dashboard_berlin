#ifndef CONNECTION_H
#define CONNECTION_H

#include <Arduino.h>

void setupWiFi();
void fetchBVGData(int currentPage);
void fetchTrafficData();
void fetchWeatherData();
void fetchNewsData();
void fetchSportsData();
void fetchStravaCombined(); 

void fetchStravaCombined(); 
bool isDeviceOnline();
#endif
