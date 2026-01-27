#include "display.h"
#include "settings.h"
#include "data.h"
#include "icons.h" 
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// Globale Instanzen nur hier im Modul
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(GxEPD2_750_GDEY075T7(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));
U8G2_FOR_ADAFRUIT_GFX u8g2;

// Fonts
#define FONT_SMALL  u8g2_font_helvB18_tf 
#define FONT_MEDIUM u8g2_font_helvB24_tf
#define FONT_LARGE  u8g2_font_helvB24_tf

void drawGlobalHeader(String title, float voltage, int nextUpdateMin) {
  // 1. Hintergrund: Schwarzer Balken über die volle Breite (60px hoch)
  display.fillRect(0, 0, 800, 60, GxEPD_BLACK);
  
  // Farben für Text im Header invertieren (Weiß auf Schwarz)
  u8g2.setBackgroundColor(GxEPD_BLACK);
  u8g2.setForegroundColor(GxEPD_WHITE);
  
  // 2. Titel (Links)
  u8g2.setFont(u8g2_font_helvB18_tf);
  // Vertikale Zentrierung bei 60px Höhe -> ca. y=42
  u8g2.setCursor(20, 42); 
  u8g2.print(title);

  // 3. Datum (Mitte/Rechts)
  // 3. Datum & Uhrzeit (Mitte/Rechts)
  // 3. Datum & Update-Zeit (Zweizeilig)
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
     // Puffer für Datum und Zeit separat
     char dateBuf[16];
     char timeBuf[16];
     
     // Datum formatieren: "Sa, 13.12."
     strftime(dateBuf, sizeof(dateBuf), "%a, %d.%m.", &timeinfo);
     // Zeit formatieren: "14:00"
     strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
     
     String d = String(dateBuf);
     // Deutsche Wochentage
     d.replace("Mon", "Mo"); d.replace("Tue", "Di"); d.replace("Wed", "Mi");
     d.replace("Thu", "Do"); d.replace("Fri", "Fr"); d.replace("Sat", "Sa"); d.replace("Sun", "So");
     
     // Zeile 1: Datum (etwas höher gesetzt, y=28)
     u8g2.setFont(u8g2_font_helvB14_tf); 
     u8g2.setCursor(500, 28); 
     u8g2.print(d);

     // Zeile 2: "Update: Uhrzeit" (darunter, y=53)
     u8g2.setFont(u8g2_font_helvB12_tf); 
     u8g2.setCursor(500, 53);
     u8g2.print("Update: " + String(timeBuf));
  }
  // 4. Batterie & Update (Rechts) - Dein Original-Code integriert
  
  // Batterie Rahmen (Weiß)
  display.drawRect(750, 25, 30, 15, GxEPD_WHITE); 
  display.fillRect(750 + 30, 29, 3, 7, GxEPD_WHITE); // Nippel
  
  // Füllstand berechnen
  float pct = (voltage - 3.6) / (4.2 - 3.6);
  if (pct < 0) pct = 0; if (pct > 1) pct = 1;
  
  // Füllung (Weiß)
  display.fillRect(752, 27, (int)(26*pct), 11, GxEPD_WHITE);

  // Update Minuten (Weiß)
  u8g2.setFont(u8g2_font_helvB10_tf); // Etwas kleiner, damit es passt
  u8g2.setCursor(750, 55); 
  u8g2.print(String(nextUpdateMin) + "m");

  // WICHTIG: Farben zurücksetzen für den Rest der Seite (Schwarz auf Weiß)
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setBackgroundColor(GxEPD_WHITE);
}
void drawWeatherIcon(int x, int y, int code) {
  int w = 100; int h = 100;
  const unsigned char* icon = icon_wolke;
  if (code == 0) icon = icon_sonne;
  else if (code >= 45 && code <= 48) icon = icon_nebel;
  else if (code >= 51 && code <= 67) icon = icon_regen;
  else if (code >= 71 && code <= 77) icon = icon_schnee;
  else if (code >= 95) icon = icon_gewitter;
  
  display.drawBitmap(x, y, icon, w, h, GxEPD_BLACK);
}

void drawWeatherSection(int x, int y) {
  drawWeatherIcon(x + 80, y + 10, cachedWeatherCode);
  
  u8g2.setFont(FONT_LARGE);
  u8g2.setCursor(x + 60, y + 150); 
  u8g2.print(String(cachedTemp, 0) + " C");
  
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(x + 40, y + 180);
  u8g2.print("Gefühlt: " + String(cachedApparent, 0) + " C");
  
  int detailY = y + 220;
  u8g2.setCursor(x + 20, detailY);
  u8g2.print("Wind: " + String(cachedWind, 0) + " km/h");
  u8g2.setCursor(x + 20, detailY + 30);
  u8g2.print("Regen: " + String(cachedRain, 1) + " mm");
  u8g2.setCursor(x + 20, detailY + 60);
  u8g2.print("Min/Max: " + String(cachedMin, 0) + "/" + String(cachedMax, 0));
}

void drawWeatherBanner(int y) {
  drawWeatherIcon(10, y + 10, cachedWeatherCode);
  u8g2.setFont(FONT_LARGE);
  u8g2.setCursor(130, y + 80); u8g2.print(String(cachedTemp, 0) + "C");
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(240, y + 50); u8g2.print("Gefühlt: " + String(cachedApparent, 0) + " C");
  u8g2.setCursor(240, y + 80); u8g2.print("Regen: " + String(cachedRain, 1) + " mm | Wind: " + String(cachedWind, 0) + " km/h");
}

void initDisplay() {
  SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  display.init(115200);
  u8g2.begin(display);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setBackgroundColor(GxEPD_WHITE);
}

void shutdownDisplay() {
  display.powerOff();
}

void drawInitMessage(String msg) {
  display.setFullWindow(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE); 
    u8g2.setFont(FONT_MEDIUM); 
    u8g2.setCursor(20, 240);
    u8g2.print(msg);
  } while (display.nextPage());
}

void drawErrorScreen(String msg) {
  display.setFullWindow(); display.firstPage(); do {
    display.fillScreen(GxEPD_WHITE); 
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setFont(FONT_MEDIUM); 
    u8g2.setCursor(50, 250); 
    u8g2.print(msg);
  } while (display.nextPage());
}

void drawDashboard(int nextUpdateMin, int currentPage, float voltage, bool slowDataUpdated) {
  display.setFullWindow(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE); 
    
    drawGlobalHeader("Berlin Traffic", voltage, nextUpdateMin);

    // LINKS: BVG
    int yPos = 80;
    if (tripCount == 0) { 
        u8g2.setFont(FONT_SMALL); u8g2.setCursor(20, 100); 
        if(currentPage > 0) u8g2.print("Keine weiteren Zuege."); else u8g2.print("Keine BVG Daten");
    }

    u8g2.setFont(FONT_MEDIUM); 
    for (int i = 0; i < tripCount; i++) {
      u8g2.setCursor(10, yPos + 25); u8g2.print(trips[i].line); 
      u8g2.setCursor(80, yPos + 25); u8g2.print(trips[i].direction); 
      u8g2.setCursor(380, yPos + 25); u8g2.print(trips[i].timeString);
      
      if (trips[i].delayMin >= 3) {
         int boxX = 475;
         display.fillRect(boxX - 2, yPos, 50, 30, GxEPD_BLACK);
         u8g2.setForegroundColor(GxEPD_WHITE); u8g2.setBackgroundColor(GxEPD_BLACK);
         u8g2.setCursor(boxX, yPos + 25); u8g2.print("+" + String(trips[i].delayMin));
         u8g2.setForegroundColor(GxEPD_BLACK); u8g2.setBackgroundColor(GxEPD_WHITE);
      } else if (trips[i].delayMin > 0) {
         u8g2.setCursor(475, yPos + 25); u8g2.print("+" + String(trips[i].delayMin));
      }
      if (i < tripCount - 1) display.drawLine(10, yPos + 35, 510, yPos + 35, GxEPD_BLACK);
      yPos += 45; 
    }
    
    // Trennlinie
    display.fillRect(530, 60, 4, 300, GxEPD_BLACK);
    // RECHTS: Wetter
    drawWeatherSection(540, 60);

    // FOOTER (TRAFFIC)
    int splitY = 370; 
    display.fillRect(0, splitY, 800, 4, GxEPD_BLACK);
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(10, splitY + 30); u8g2.print("Auto nach:");
    u8g2.setCursor(10, splitY + 60); u8g2.print(DESTINATION); 
    if (slowDataUpdated) display.fillCircle(10 + 350, splitY + 55, 3, GxEPD_BLACK);
    u8g2.setFont(FONT_LARGE); 
    u8g2.setCursor(550, splitY + 60); u8g2.print(cachedTrafficTime);

  } while (display.nextPage());
}

void drawNewsPage(int nextUpdateMin, float voltage) {
  display.setFullWindow(); display.firstPage();
  do {
    drawGlobalHeader("Tagesschau", voltage, nextUpdateMin);

    int yPos = 80;
    if (newsCount == 0) { u8g2.setFont(FONT_SMALL); u8g2.setCursor(20, 100); u8g2.print("Lade Daten..."); }

    u8g2.setFont(FONT_SMALL); 
    for (int i = 0; i < newsCount; i++) {
      u8g2.setCursor(10, yPos + 10); u8g2.print(newsList[i].time); 
      u8g2.setCursor(90, yPos + 10);
      String text = newsList[i].headline;
      if (text.length() > 60) text = text.substring(0, 58) + "...";
      u8g2.print(text);
      if (i < newsCount - 1) display.drawLine(10, yPos + 25, 790, yPos + 25, GxEPD_BLACK);
      yPos += 50; 
    }
    int splitY = 370; 
    display.drawLine(0, splitY, 800, splitY, GxEPD_BLACK);
    drawWeatherBanner(splitY);
  } while (display.nextPage());
}
// --- NEU: SPORT SEITE ---
void drawSportsPage(int nextUpdateMin, float voltage) {
  display.setFullWindow(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE); 
    
    // Header (Standard wie andere Seiten)
    drawGlobalHeader("Sport", voltage, nextUpdateMin);
    int yPos = 90;

    // 1. FORMEL 1 SEKTION
    if (nextF1.hasRace) {
       u8g2.setFont(FONT_MEDIUM); 
       u8g2.setCursor(10, yPos); u8g2.print("F1: " + nextF1.raceName);
       
       u8g2.setFont(FONT_SMALL);
       u8g2.setCursor(10, yPos + 30); u8g2.print(nextF1.circuit);
       
       // Rechtsbündig Zeiten
       u8g2.setCursor(450, yPos); u8g2.print(nextF1.qualiTime);
       u8g2.setCursor(450, yPos + 30); u8g2.print(nextF1.raceTime);
       
       // Trennlinie
       yPos += 45;
       display.fillRect(10, yPos, 780, 2, GxEPD_BLACK);
       yPos += 30; // Abstand zur Bundesliga
    } else {
       u8g2.setFont(FONT_SMALL);
       u8g2.setCursor(10, yPos); u8g2.print("Kein F1 Rennen diese Woche.");
       yPos += 40;
    }

    // 2. BUNDESLIGA SEKTION
    u8g2.setFont(FONT_MEDIUM);
    u8g2.setCursor(10, yPos); u8g2.print("Bundesliga Spieltag");
    yPos += 35;

    u8g2.setFont(FONT_SMALL);
    int col2X = 420; // Start zweite Spalte
    int startY = yPos;
    
    for (int i = 0; i < blMatchCount; i++) {
       // Wir machen 2 Spalten Layout um Platz zu sparen
       // Wenn i gerade (0,2,4) -> Linke Spalte
       // Wenn i ungerade (1,3,5) -> Rechte Spalte
       
       int x = (i % 2 == 0) ? 10 : col2X;
       int y = startY + ((i / 2) * 30); // Jede Zeile 30px Höhe
       
       String matchStr = blMatches[i].timeString + ": " + blMatches[i].homeTeam + " - " + blMatches[i].guestTeam;
       
       // String kürzen falls zu lang
       if (matchStr.length() > 35) matchStr = matchStr.substring(0, 34);
       
       u8g2.setCursor(x, y);
       u8g2.print(matchStr);
    }
    int splitY = 370; 
    display.drawLine(0, splitY, 800, splitY, GxEPD_BLACK);
    drawWeatherBanner(splitY);
  } while (display.nextPage());
}

void drawLowBatteryScreen(float voltage) {
  display.setFullWindow(); 
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Großes Warnsymbol oder Text
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB24_tf); 
    u8g2.setCursor(250, 200);
    u8g2.print("BATTERIE LEER!");
    
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(290, 250);
    u8g2.print(String(voltage) + " V");

    u8g2.setFont(u8g2_font_helvB14_tf);
    u8g2.setCursor(230, 300);
    u8g2.print("Bitte aufladen...");
    
    // Optional: Batterie-Icon zeichnen (leer)
    display.drawRect(350, 100, 100, 50, GxEPD_BLACK);
    display.fillRect(450, 115, 10, 20, GxEPD_BLACK);
    // Ein rotes "X" (hier schwarz) durchstreichen
    display.drawLine(350, 100, 450, 150, GxEPD_BLACK);
    display.drawLine(350, 150, 450, 100, GxEPD_BLACK);

  } while (display.nextPage());
}
void drawStravaCombinedPage(int nextUpdateMin, float voltage) {
  display.setFullWindow(); display.firstPage();
  do {
    // 1. Header
    drawGlobalHeader("Strava Dashboard", voltage, nextUpdateMin);
    
    // Fallback: Keine Daten
    if (!latestStrava.hasData && !stravaStats.hasData) {
       u8g2.setFont(FONT_MEDIUM); 
       u8g2.setCursor(20, 200); u8g2.print("Lade Strava Daten...");
       continue;
    }

    // ==========================================
    // BEREICH 1: LETZTE AKTIVITÄT (OBEN)
    // ==========================================
    int yTop = 90;
    
    // Überschrift mit Datum und Typ
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(10, yTop);
    u8g2.print("Letzte: " + latestStrava.date + " " + latestStrava.type);
    
    // Name der Aktivität (etwas kleiner, falls lang)
    u8g2.setFont(u8g2_font_helvB14_tf); 
    String name = latestStrava.name;
    if(name.length() > 55) name = name.substring(0, 52) + "...";
    u8g2.setCursor(10, yTop + 30);
    u8g2.print(name);

    int yBig = yTop + 85;
    
    u8g2.setFont(u8g2_font_helvB24_tf);
    u8g2.setCursor(10, yBig); u8g2.print(latestStrava.distance);
    
    // Zeit & Pace (Mittel)
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(250, yBig); u8g2.print(latestStrava.duration);
    u8g2.setCursor(450, yBig); u8g2.print(latestStrava.paceOrSpeed);

    // ==========================================
    // TRENNLINIE
    // ==========================================
    int ySplit = 230;
    display.fillRect(0, ySplit, 800, 4, GxEPD_BLACK);

    // ==========================================
    // BEREICH 2: STATISTIKEN (UNTEN)
    // ==========================================
    // Wir nutzen den Platz bis 480px. Keine Wetteranzeige!
    
    int yStats = ySplit + 40;
    int colRun = 10;
    int colRide = 410;
    
    // Vertikale Trennlinie für Stats
    display.fillRect(400, ySplit, 2, 250, GxEPD_BLACK);

    // --- SPALTE LINKS: LAUFEN ---
    u8g2.setFont(FONT_MEDIUM); 
    u8g2.setCursor(colRun, yStats); u8g2.print("LAUFEN");
    
    u8g2.setFont(u8g2_font_helvB18_tf);
    // Monat (4 Wochen)
    u8g2.setCursor(colRun, yStats + 50); u8g2.print("4 Wo: " + String(stravaStats.recentRunDist, 1) + " km");
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(colRun + 220, yStats + 50); u8g2.print("(" + String(stravaStats.recentRunCount) + ")"); // Anzahl

    // Jahr
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(colRun, yStats + 100); u8g2.print("Jahr: " + String(stravaStats.yearRunDist, 0) + " km");

    // --- SPALTE RECHTS: RAD ---
    u8g2.setFont(FONT_MEDIUM); 
    u8g2.setCursor(colRide, yStats); u8g2.print("RAD");

    u8g2.setFont(u8g2_font_helvB18_tf);
    // Monat
    u8g2.setCursor(colRide, yStats + 50); u8g2.print("4 Wo: " + String(stravaStats.recentRideDist, 0) + " km");
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(colRide + 220, yStats + 50); u8g2.print("(" + String(stravaStats.recentRideCount) + ")");

    // Jahr
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(colRide, yStats + 100); u8g2.print("Jahr: " + String(stravaStats.yearRideDist, 0) + " km");

  } while (display.nextPage());
}