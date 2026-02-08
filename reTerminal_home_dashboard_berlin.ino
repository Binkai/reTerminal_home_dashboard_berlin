/**
 * Project: ESP32-S3 E1001 - HomeDashboard Berlin
 * @author Kai Kuhfeld
 * @brief Home-Dashboard für das reTerminal E1001 von seeedStudios mit: BVG, Google Maps, Strava, F1, Bundesliga, Tagesschau, Wetter, lokaler Ping
 * @date 2026-01-26
 */
#include <Arduino.h>
#include <WiFi.h>
#include "settings.h"
#include "data.h"
#include "utils.h"
#include "connection.h"
#include "display.h"
#define SERIAL_RX 44
#define SERIAL_TX 43
// ==============================
// RTC SPEICHER (Definition)
// ==============================
RTC_DATA_ATTR long lastSlowUpdateTime = 0;        
RTC_DATA_ATTR int currentPage = 0; 
RTC_DATA_ATTR char cachedTrafficTime[32] = "-- min"; 
RTC_DATA_ATTR char cachedTrafficRoute[64] = "Lade..."; 
RTC_DATA_ATTR int cachedWeatherCode = 0;
RTC_DATA_ATTR float cachedTemp = 0.0;
RTC_DATA_ATTR float cachedApparent = 0.0;
RTC_DATA_ATTR float cachedWind = 0.0;
RTC_DATA_ATTR float cachedRain = 0.0;
RTC_DATA_ATTR float cachedMin = 0.0;
RTC_DATA_ATTR float cachedMax = 0.0;
RTC_DATA_ATTR bool cachedSportWeekend = false;

// Globale Puffer
bool isSportWeekend = false; // Wird im Setup aus Cache oder Network befüllt
// ...
// Globale Puffer (werden bei jedem Reset neu befüllt)
BvgTrip trips[6]; 
int tripCount = 0;
NewsItem newsList[6]; 
int newsCount = 0;
BundesligaMatch blMatches[9];
int blMatchCount = 0;
F1Race nextF1;

// Lokale Variablen für Setup
int currentSleepMin = INTERVAL_NORMAL;
float batteryVoltage = 0.0;
bool slowDataUpdated = false;
bool manualUpdate = false; 
void setup() {
  Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
  Serial1.println("\n\n--- SYSTEM START V30 (FIXED) ---");
  
  pinMode(KEY0, INPUT_PULLUP);
  pinMode(KEY1, INPUT_PULLUP);
  pinMode(KEY2, INPUT_PULLUP);

  // 1. WAKEUP CHECK & NAVIGATION START
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool btnGreen = false;

  // Standardmäßig behalten wir die letzte Seite bei, es sei denn...
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    manualUpdate = true;
    uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
    
    if (wakeup_pin_mask & (1ULL << KEY0)) { 
        // Taste 0: Reset auf Dashboard & Turbo Update
        lastSlowUpdateTime = 0; // Erzwingt Daten-Update
        btnGreen = true; 
    }
    else if (wakeup_pin_mask & (1ULL << KEY1)) { 
        // Taste 1: Blättern
        currentPage++; 
        if (currentPage > 3) currentPage = 0; // 0->1->2->3->0
    }
    else if (wakeup_pin_mask & (1ULL << KEY2)) { 
        // Taste 2 (Optional): Reset
    }
  }

  initDisplay();
  batteryVoltage = readBattery();
  Serial1.print("Batterie-Spannung: ");
  Serial1.println(batteryVoltage);
  // --- BATTERIE SCHUTZ ---
  if (batteryVoltage < 3.45) {
      Serial1.println("KRITISCHE SPANNUNG! Zeige Warnung und gehe schlafen.");
      drawLowBatteryScreen(batteryVoltage);
      shutdownDisplay();
      // Wir gehen in den "ewigen" Schlaf (kein Timer Wakeup), bis Reset gedrückt wird oder Strom kommt
      esp_deep_sleep_start(); 
  }
  // -----------------------
  if (manualUpdate) {
    // Info zeigen, wo wir gerade hin navigieren
    String pName = "Dashboard";
    if (currentPage == 1) pName = "News";
    else if (currentPage == 2) pName = "Sport";
    else if (currentPage == 3) pName = "Strava"; // <--- NEU
    if (btnGreen) drawInitMessage("Turbo Update...");
    else drawInitMessage("Lade " + pName + "...");
  }

  // 2. WIFI & ZEIT
  setupWiFi(); 
  
  if (WiFi.status() != WL_CONNECTED) {
    drawErrorScreen("WLAN Fehler!");
    esp_deep_sleep(10 * 60e6);
  }
  
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  
  struct tm timeinfo;
  time_t now;
  bool timeValid = getLocalTime(&timeinfo);
  time(&now);
  
  // Sleep Zeit berechnen
  if(timeValid) {
     currentSleepMin = calculateSleepTime(timeinfo);
  } else {
     currentSleepMin = 30; // Fallback
  }
// 3. SEITEN-AUTOMATIK
  if (!manualUpdate && timeValid) {
       int h = timeinfo.tm_hour;
       int wDay = timeinfo.tm_wday; // 0=So, 1=Mo...

       // Werktags (Mo-Do) definieren
       bool isWorkDay = (wDay >= 1 && wDay <= 4);
       
       // Standardmäßig: Rotation aktivieren
       bool enableRotation = true; 

       // Spezial-Regel: Arbeitszeit
       if (isWorkDay) {
           if (h >= 6 && h < 9) {
               enableRotation = false; // Erzwinge Traffic Seite (Page 0)
           }
       }

       // --- ANWENDUNG DER LOGIK ---
       if (enableRotation) {
           // Rotation: Nächste Seite
           currentPage++;
           if (currentPage > 3) currentPage = 0;
       } else {
           // Keine Rotation: Fest auf Dashboard (Traffic)
           currentPage = 0;
       }
  }
  // 4. DATEN HOLEN 
  
  // A) Langsame Daten (Wetter, Traffic, Sport) -> Alle 30min ODER bei Button 0 (Turbo)
  // Wir holen Sportdaten IMMER im Slow-Zyklus, damit sie im Cache sind.
  if ((now - lastSlowUpdateTime > SLOW_DATA_UPDATE_SEC) || (lastSlowUpdateTime == 0)) {
    Serial1.println("Hole Slow Data...");
    fetchWeatherData();
    
    // Traffic nur für Dashboard nötig
    fetchTrafficData(); 
    
    // Sport holen und Cache aktualisieren
    fetchSportsData(); // Diese Funktion setzt 'isSportWeekend' (global) und schreibt in Cache
    cachedSportWeekend = isSportWeekend;
    
    lastSlowUpdateTime = now; 
    slowDataUpdated = true;
  } else {
    // Wenn kein Update lief, Zustand aus Cache wiederherstellen
    isSportWeekend = cachedSportWeekend;
  }

  // B) Seitenspezifische Daten (Müssen IMMER geholt werden für die aktive Seite)
  if (currentPage == 0) {
      fetchBVGData(currentPage); 
  } 
  else if (currentPage == 1) {
      fetchNewsData();
  }
  
  if (currentPage == 2 && blMatchCount == 0 && nextF1.raceName[0] == 0) {
       Serial1.println("Seite 2 aktiv aber leer -> Lade Sport nach!");
       fetchSportsData();
  }
  if (currentPage == 3) { fetchStravaCombined(); } 

  // Offline gehen
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // 5. ZEICHNEN
  Serial1.print("Zeichne Seite: "); Serial1.println(currentPage);
  
  if (currentPage == 0) {
      drawDashboard(currentSleepMin, currentPage, batteryVoltage, slowDataUpdated);
  } else if (currentPage == 1) {
      drawNewsPage(currentSleepMin, batteryVoltage);
  } else if (currentPage == 2) {
      drawSportsPage(currentSleepMin, batteryVoltage);
  } else if (currentPage == 3) {
    drawStravaCombinedPage(currentSleepMin, batteryVoltage);
  }
  
  // Shutdown
  shutdownDisplay(); 

  // Sleep Setup
  uint64_t gpio_mask = (1ULL << KEY0) | (1ULL << KEY1) | (1ULL << KEY2);
  esp_sleep_enable_ext1_wakeup(gpio_mask, ESP_EXT1_WAKEUP_ANY_LOW);
  uint64_t sleepTimeMicro = (uint64_t)currentSleepMin * 60ULL * 1000000ULL;
  
  Serial1.println("Gehe schlafen...");
  esp_deep_sleep(sleepTimeMicro);
}

void loop() {}