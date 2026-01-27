#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <time.h>

String cleanText(String text);
float readBattery();
int calculateSleepTime(struct tm timeinfo);

#endif