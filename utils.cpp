#include "utils.h"
#include "settings.h"

String cleanText(String text) {
  text.replace("&quot;", "\"");
  text.replace("&#039;", "'");
  text.replace("Rathaus", "Rath.");
  text.replace("Bahnhof", "Bhf");
  text.replace("Flughafen", "BER");
  text.replace("Berlin", ""); 
  text.replace("  ", " ");    
  return text;
}

float readBattery() {
  pinMode(BAT_EN_PIN, OUTPUT);
  digitalWrite(BAT_EN_PIN, HIGH); 
  delay(10); 
  int mv = analogReadMilliVolts(BAT_ADC_PIN);
  digitalWrite(BAT_EN_PIN, LOW);
  return (mv / 1000.0f) * 2.0f;
}

int calculateSleepTime(struct tm t) {
  int hour = t.tm_hour;
  if (hour >= 7 && hour < 9) return INTERVAL_FAST;
  if (hour >= 23 || hour < 6) return INTERVAL_NIGHT;
  return INTERVAL_NORMAL;
}