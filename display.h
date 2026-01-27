#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

void initDisplay();
void drawInitMessage(String msg);
void drawErrorScreen(String msg);
void drawDashboard(int nextUpdateMin, int currentPage, float voltage, bool slowDataUpdated);
void drawNewsPage(int nextUpdateMin, float voltage);
void shutdownDisplay();
void drawSportsPage(int nextUpdateMin, float voltage);
void drawLowBatteryScreen(float voltage); // <--- NEU
void drawStravaCombinedPage(int nextUpdateMin, float voltage);
#endif