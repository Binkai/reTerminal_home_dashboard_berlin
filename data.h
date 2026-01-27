#ifndef DATA_H
#define DATA_H

#include <Arduino.h>

struct BvgTrip {
  String line;
  String direction;
  String timeString;
  int delayMin;
};

struct NewsItem { 
  String time; 
  String headline; 
};

extern BvgTrip trips[6];
extern int tripCount;
extern NewsItem newsList[6];
extern int newsCount;

extern char cachedTrafficTime[32]; 
extern char cachedTrafficRoute[64]; 
extern int cachedWeatherCode;
extern float cachedTemp;
extern float cachedApparent;
extern float cachedWind;
extern float cachedRain;
extern float cachedMin;
extern float cachedMax;
struct BundesligaMatch {
  String homeTeam;
  String guestTeam;
  String timeString; // z.B. "Sa 15:30"
  bool isTopMatch;   // Optional für Hervorhebung
};

struct F1Race {
  bool hasRace;      // Gibt es an diesem WE ein Rennen?
  String raceName;   // z.B. "Grand Prix von Monaco"
  String circuit;    // z.B. "Monte Carlo"
  String qualiTime;  // "Sa 16:00"
  String raceTime;   // "So 15:00"
  char date[16];     // NEU: z.B. "16.03."
};

// Globale Sport-Variablen
extern BundesligaMatch blMatches[9]; // Max 9 Spiele pro Spieltag
extern int blMatchCount;
extern F1Race nextF1;

extern bool isSportWeekend; 
extern bool cachedSportWeekend; 

struct StravaActivity {
  bool hasData;
  String name;         // "Abendlauf"
  String type;         // "Run", "Ride"
  String distance;     // "5.2 km"
  String duration;     // "0h 32m"
  String date;         // "14.12."
  String paceOrSpeed;  // z.B. "5:30 /km" oder "25.0 km/h"
};

extern StravaActivity latestStrava;

struct StravaStats {
  bool hasData;
  // Letzte 4 Wochen (als "Monat" interpretiert)
  float recentRunDist;  // km
  int recentRunTime;    // Stunden
  int recentRunCount;   // Anzahl Läufe
  
  float recentRideDist; // km
  int recentRideTime;   // Stunden
  int recentRideCount;
  
  // Dieses Jahr (YTD)
  float yearRunDist;
  float yearRideDist;
};

extern StravaStats stravaStats; 
// ...
// ...
#endif