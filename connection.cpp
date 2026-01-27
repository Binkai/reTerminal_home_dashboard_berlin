#include "connection.h"
#include "settings.h"
#include "data.h"
#include "utils.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESPping.h>

void setupWiFi() {
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    // Serial Ausgabe falls nötig
  }
  Serial1.println("Verbinde WLAN..");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 60) {
    delay(100); 
    Serial1.print(".");
    retry++;
  }
  Serial1.println("\nVerbunden! IP: " + WiFi.localIP().toString());
    delay(500);
}

void fetchBVGData(int currentPage) {
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://v6.vbb.transport.rest/stops/" + STOP_ID + "/departures?results=30&duration=240&suburban=false&bus=false&tram=false&regional=false&express=false&ferry=false";
  http.begin(client, url);
  int httpCode = http.GET();
  Serial1.println("BVG - Datenabruf - Beginn");
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial1.printf("Payload Größe: %d bytes\n", payload.length()); // Kommen Daten an?
    DynamicJsonDocument filter(512);
    filter["departures"][0]["line"]["name"] = true;
    filter["departures"][0]["direction"] = true;
    filter["departures"][0]["when"] = true;
    filter["departures"][0]["delay"] = true;
    Serial1.printf("HTTP Code: %d\n", httpCode); // Zeigt uns den Status

    // RAM Optimierung: Dokument etwas kleiner gewählt
    DynamicJsonDocument doc(40000); 
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    
    if (!error) {
      JsonArray departures = doc["departures"];
      tripCount = 0; 
      time_t nowTime; time(&nowTime);
      int validIndex = 0; 
      int skipCount = currentPage * 6; 
      
      for (JsonObject dep : departures) {
        if (tripCount >= 6) break;
        String rawTime = dep["when"].as<String>();
        struct tm tm_dep = {0}; strptime(rawTime.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_dep);
        time_t t_dep = mktime(&tm_dep);
        double diffSeconds = difftime(t_dep, nowTime);
        int minutesLeft = diffSeconds / 60;
        
        if (minutesLeft < MIN_WALKING_MIN) continue; 
        if (validIndex < skipCount) { validIndex++; continue; } 
        
        trips[tripCount].line = dep["line"]["name"].as<String>();
        String rawDir = dep["direction"].as<String>();
        trips[tripCount].direction = cleanText(rawDir);
        
        if (trips[tripCount].direction.length() > 16) {
           trips[tripCount].direction = trips[tripCount].direction.substring(0, 16);
        }

        if (rawTime.length() >= 16) trips[tripCount].timeString = rawTime.substring(11, 16);
        else trips[tripCount].timeString = "??:??";
        
        int delaySec = dep["delay"] | 0; 
        trips[tripCount].delayMin = delaySec / 60;
        
        tripCount++; validIndex++;
      }
    } else {
      Serial1.println("BVG JSON Fehler: ");
      Serial.println(error.c_str());
    }
  }
  Serial1.println("BVG - Datenabruf - Beginn");
  http.end();
}

void fetchTrafficData() {
  if (GOOGLE_API_KEY.length() < 10) { strcpy(cachedTrafficTime, "No Key"); return; }
  Serial1.println("GoogleMaps - Traffic - Datenabruf - Beginn");
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String safeOrigin = ORIGIN; safeOrigin.replace(" ", "%20");
  String safeDest = DESTINATION; safeDest.replace(" ", "%20");
  String url = "https://maps.googleapis.com/maps/api/distancematrix/json?origins=" + safeOrigin + "&destinations=" + safeDest + "&departure_time=now&traffic_model=best_guess&language=de&key=" + GOOGLE_API_KEY;
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument filter(256);
    filter["rows"][0]["elements"][0]["duration_in_traffic"]["text"] = true;
    filter["rows"][0]["elements"][0]["duration"]["text"] = true;
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!error) {
      const char* t_traffic = doc["rows"][0]["elements"][0]["duration_in_traffic"]["text"];
      const char* t_normal  = doc["rows"][0]["elements"][0]["duration"]["text"];
      if (t_traffic) {
        String s = String(t_traffic); s.replace("mins", "min"); s.toCharArray(cachedTrafficTime, 32); 
        strcpy(cachedTrafficRoute, "Verkehrslage:"); 
      } else if (t_normal) {
        String s = String(t_normal); s.replace("mins", "min"); s.toCharArray(cachedTrafficTime, 32);
        strcpy(cachedTrafficRoute, "Freie Fahrt:");
      }
    }
  }
  Serial1.println("GoogleMaps - Traffic - Datenabruf - Ende");
  http.end();
}

void fetchWeatherData() {
  Serial1.println("Wetter  - Datenabruf - Beginn");
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + LAT + "&longitude=" + LON + "&current=temperature_2m,weather_code,apparent_temperature,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,precipitation_sum&timezone=Europe%2FBerlin&forecast_days=1";
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(8192); 
    DeserializationError error = deserializeJson(doc, payload); 
    if (!error) {
      cachedTemp = doc["current"]["temperature_2m"];
      cachedWeatherCode = doc["current"]["weather_code"];
      cachedApparent = doc["current"]["apparent_temperature"];
      cachedWind = doc["current"]["wind_speed_10m"];
      cachedRain = doc["daily"]["precipitation_sum"][0];
      cachedMax = doc["daily"]["temperature_2m_max"][0];
      cachedMin = doc["daily"]["temperature_2m_min"][0];
    }
  }
  http.end();
    Serial1.println("Wetter  - Datenabruf - Ende");
}

void fetchNewsData() {
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://www.tagesschau.de/api2u/homepage";
  http.begin(client, url);
  http.setUserAgent("Mozilla/5.0 (ESP32)");
  http.setTimeout(10000); 
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument filter(512);
    filter["news"][0]["title"] = true;
    filter["news"][0]["date"] = true;
    DynamicJsonDocument doc(30000); 
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!error) {
      JsonArray news = doc["news"];
      newsCount = 0;
      if (!news.isNull()) {
        for (JsonObject n : news) {
          if (newsCount >= 5) break;
          String rawTitle = n["title"].as<String>();
          String rawDate = n["date"].as<String>(); 
          newsList[newsCount].headline = cleanText(rawTitle); 
          if(rawDate.length() > 16) newsList[newsCount].time = rawDate.substring(11, 16);
          else newsList[newsCount].time = "--:--";
          newsCount++;
        }
      }
    }
  }
  http.end();
}
void fetchSportsData() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    // Variablen für Zeitvergleich
    time_t now;
    time(&now);
    bool eventFound = false; 

    Serial1.println("--------------------------------");
    Serial1.println("DEBUG: 1. Start fetchSportsData");

    if (WiFi.status() == WL_CONNECTED) {
        Serial1.println("DEBUG: 2. WiFi ist verbunden");

        String f1Url = "https://api.jolpi.ca/ergast/f1/current/next.json";
        
        http.begin(client, f1Url);
        nextF1.hasRace = false; 

        Serial1.println("DEBUG: 3. Sende GET Request...");
        int httpResponseCode = http.GET();

        Serial1.print("DEBUG: 4. Response Code erhalten: ");
        Serial1.println(httpResponseCode);

        if (httpResponseCode > 0) {
            Serial1.println("DEBUG: 5. Daten empfangen, verarbeite JSON...");
            String payload = http.getString();
            
            // Speicher ggf. erhöhen, falls die API mehr sendet
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                Serial1.print("DEBUG: JSON Fehler: ");
                Serial1.println(error.c_str());
                http.end();
                return;
            }

            // Das Haupt-Objekt für spätere Nutzung
            JsonObject mainRaceObj = doc["MRData"]["RaceTable"]["Races"][0];

            Serial1.println("\n--- F1 DEBUG START ---");

            // 1. Zeit prüfen
            struct tm timeinfo;
            bool timeValid = getLocalTime(&timeinfo);
            int cYear = 0, cMonth = 0, cDay = 0;

            if(timeValid){
                char debugToday[15];
                strftime(debugToday, sizeof(debugToday), "%Y-%m-%d", &timeinfo);
                Serial1.print("DEBUG: ESP32 Datum: ");
                Serial1.println(debugToday);
                
                cYear = timeinfo.tm_year + 1900;
                cMonth = timeinfo.tm_mon + 1;
                cDay = timeinfo.tm_mday;
            } else {
                Serial1.println("DEBUG: FEHLER - Keine NTP Zeit!");
            }

            // 2. API Inhalt prüfen
            JsonArray races = doc["MRData"]["RaceTable"]["Races"];
            Serial1.print("DEBUG: Rennen gefunden: ");
            Serial1.println(races.size());

            // 3. Schleife (Nimm Variable 'r' statt 'race' um Konflikte zu vermeiden)
            for (JsonObject r : races) {
                const char* rDate = r["date"];
                const char* rName = r["raceName"];
                
                Serial1.print("DEBUG: API liefert: ");
                Serial1.print(rName);
                Serial1.print(" -> ");
                Serial1.println(rDate);

                // Datum parsen
                int rYear, rMonth, rDay;
                sscanf(rDate, "%d-%d-%d", &rYear, &rMonth, &rDay);

                // Vergleich nur machen, wenn wir gültige Zeit haben
                if (timeValid) {
                    if (rDay == cDay && rMonth == cMonth && rYear == cYear) {
                         Serial1.println("       -> STATUS: HEUTE!");
                    } else if (rMonth == cMonth) {
                         Serial1.println("       -> STATUS: Dieser Monat, aber nicht heute.");
                    } else {
                         Serial1.println("       -> STATUS: Anderer Monat (Zukunft oder Vergangenheit).");
                    }
                }
            }
            Serial1.println("--- F1 DEBUG ENDE ---\n");
            // --- DEBUG ENDE ---

            if (!mainRaceObj.isNull()) {
                nextF1.raceName = cleanText(mainRaceObj["raceName"].as<String>());
                if (mainRaceObj.containsKey("Circuit")) {
                    nextF1.circuit = cleanText(mainRaceObj["Circuit"]["circuitName"].as<String>());
                }

                String dateStr = mainRaceObj["date"].as<String>();
                String timeStr = mainRaceObj["time"].as<String>();

                struct tm tm_race = {0};
                strptime(dateStr.c_str(), "%Y-%m-%d", &tm_race);
                tm_race.tm_hour = 12; 
                time_t raceTime = mktime(&tm_race);

                double diff = difftime(raceTime, now);

                // Logik: Anzeigen (-12h bis +4 Tage)
                if (diff > -43200 && diff < 345600) { 
                    nextF1.hasRace = true;
                    eventFound = true;
                } else {
                    // Daten da, aber nicht dieses Wochenende
                    nextF1.hasRace = true; 
                }

      int y, M, d, h, m, s;
      // API Format ist: "2025-03-16" und "14:00:00Z"
      sscanf(dateStr.c_str(), "%d-%d-%d", &y, &M, &d);
      sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &s);

      struct tm tm_utc = {0};
      tm_utc.tm_year = y - 1900; // Jahre seit 1900
      tm_utc.tm_mon  = M - 1;    // Monate 0-11
      tm_utc.tm_mday = d;
      tm_utc.tm_hour = h;
      tm_utc.tm_min  = m;
      tm_utc.tm_sec  = s;
      tm_utc.tm_isdst = 0;       // API sendet immer UTC, also kein DST

      char oldTZ[64];
      strcpy(oldTZ, getenv("TZ")); 
      setenv("TZ", "UTC0", 1); 
      tzset();
      time_t t_race = mktime(&tm_utc); 
      
      // 2. TZ wiederherstellen (auf Berlin)
      setenv("TZ", oldTZ, 1);
      tzset();
      
      // 3. Jetzt als Lokalzeit auslesen
      struct tm *tm_local = localtime(&t_race);
      
      // 4. String bauen
      char timeBuf[10];
      char dateBuf[10];
      strftime(timeBuf, sizeof(timeBuf), "%H:%M", tm_local);
      strftime(dateBuf, sizeof(dateBuf), "%d.%m.", tm_local);
      
      // Ergebnis: "Rennen: 16.03. 15:00 Uhr"

// --- NEU: Datum formatieren ---
      String dayStr = dateStr.substring(8, 10);
      String monStr = dateStr.substring(5, 7);
      String shortDate = dayStr + "." + monStr + "."; 

      nextF1.raceTime = "Rennen: " + String(dateBuf) + " " + String(timeBuf) + " Uhr";
      
      if (mainRaceObj.containsKey("Qualifying")) {
          String qTime = mainRaceObj["Qualifying"]["time"].as<String>();
                   
          int qHour = qTime.substring(0,2).toInt() + 1;
          String qDisplayHour = String(qHour);
          if (qHour < 10) qDisplayHour = "0" + qDisplayHour;

          nextF1.qualiTime = "Quali: " + qDisplayHour + ":" + qTime.substring(3,5) + " Uhr";
      } else {
          nextF1.qualiTime = "Quali: --:--";
      }
    
            }
        }
        http.end();
    } else {
        Serial1.println("DEBUG: WiFi nicht verbunden!");
    }
            

  // 2. BUNDESLIGA
String blUrl = "https://api.openligadb.de/getmatchdata/bl1";
  http.begin(client, blUrl);
  Serial1.println("BuLi - Datenabruf - Beginn");
  int httpCode = http.GET();
  Serial1.printf("HTTP Code: %d\n", httpCode); 
  blMatchCount = 0;
  if (httpCode == 200) {
    String payload = http.getString();
    Serial1.printf("Payload Größe: %d bytes\n", payload.length()); 
    // FILTER ANPASSEN: Wir brauchen Ergebnisse und Status!
    DynamicJsonDocument filter(512);
    filter[0]["team1"]["shortName"] = true;
    filter[0]["team2"]["shortName"] = true;
    filter[0]["matchDateTime"] = true; 
    filter[0]["matchIsFinished"] = true; 
    filter[0]["matchResults"] = true;    
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    
    if (!error) {
      JsonArray arr = doc.as<JsonArray>();
      
      // PRÜFUNG: Ist das erste Match dieses Spieltags nahe?
      if (arr.size() > 0) {
          String firstMatchDate = arr[0]["matchDateTime"].as<String>();
          struct tm tm_match = {0};
          strptime(firstMatchDate.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_match);
          time_t matchTime = mktime(&tm_match);
          double diff = difftime(matchTime, now);
          
          if (diff > -172800 && diff < 345600) {
              eventFound = true;
          }
      }

      for (JsonObject match : arr) {
        if (blMatchCount >= 9) break;
        
        String t1 = match["team1"]["shortName"].as<String>();
        String t2 = match["team2"]["shortName"].as<String>();
        
        blMatches[blMatchCount].homeTeam = cleanText(t1);
        blMatches[blMatchCount].guestTeam = cleanText(t2);
        
        bool isFinished = match["matchIsFinished"];
        
        if (isFinished) {
            int score1 = 0;
            int score2 = 0;
            for (JsonObject res : match["matchResults"].as<JsonArray>()) {
                if (res["resultTypeID"] == 2) {
                    score1 = res["pointsTeam1"];
                    score2 = res["pointsTeam2"];
                    break;
                }
            }
            // Ergebnis in den timeString schreiben (z.B. "3:1")
            blMatches[blMatchCount].timeString = String(score1) + ":" + String(score2);
            
        } else {
            String dt = match["matchDateTime"].as<String>(); 
            struct tm tm_match = {0};
            strptime(dt.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_match);
            char buffer[16];
            strftime(buffer, sizeof(buffer), "%a %H:%M", &tm_match); 
            String sDate = String(buffer);
            sDate.replace("Fri", "Fr"); sDate.replace("Sat", "Sa"); sDate.replace("Sun", "So");
            
            blMatches[blMatchCount].timeString = sDate;
        }

        blMatchCount++;
      }
    } else {
      Serial1.println("JSON Fehler: ");
      Serial.println(error.c_str()); 
    }
      Serial1.println("BuLi - Datenabruf - Ende");
  }
  http.end();
  // Ergebnis speichern
  isSportWeekend = eventFound;
  cachedSportWeekend = eventFound; 
}
StravaActivity latestStrava;
StravaStats stravaStats;

// Interne Helper-Funktion (nicht im Header, nur hier)
String getStravaTokenInternal() {
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String authUrl = "https://www.strava.com/oauth/token";
  authUrl += "?client_id=" + STRAVA_CLIENT_ID + "&client_secret=" + STRAVA_CLIENT_SECRET;
  authUrl += "&refresh_token=" + STRAVA_REFRESH_TOKEN + "&grant_type=refresh_token";
  
  http.begin(client, authUrl);
  int httpCode = http.POST("");
  String token = "";
  if (httpCode == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    token = doc["access_token"].as<String>();
  }
  http.end();
  return token;
}

// --- HAUPTFUNKTION FÜR Strava ---
void fetchStravaCombined() {
  Serial1.println("Strava Combined - Start...");
  latestStrava.hasData = false;
  stravaStats.hasData = false;

  // 1. Token holen (nur 1x!)
  String token = getStravaTokenInternal();
  if (token == "") { Serial1.println("Strava Auth Fehler"); return; }

  WiFiClientSecure client; client.setInsecure(); HTTPClient http;

  // -----------------------------
  // TEIL A: Letzte Aktivität holen
  // -----------------------------
  String actUrl = "https://www.strava.com/api/v3/athlete/activities?per_page=1";
  http.begin(client, actUrl);
  http.addHeader("Authorization", "Bearer " + token);
  int codeAct = http.GET();
  
  if (codeAct == 200) {
    String payload = http.getString();
    DynamicJsonDocument filter(512);
    filter[0]["name"] = true; filter[0]["distance"] = true; 
    filter[0]["moving_time"] = true; filter[0]["type"] = true; filter[0]["start_date_local"] = true;
    
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    
    if (doc.size() > 0) {
       JsonObject a = doc[0];
       latestStrava.name = cleanText(a["name"].as<String>());
       latestStrava.type = a["type"].as<String>();
       
       float d = a["distance"].as<float>();
       latestStrava.distance = String(d / 1000.0, 1) + " km";
       
       int s = a["moving_time"];
       latestStrava.duration = String(s/3600) + "h " + String((s%3600)/60) + "m";
       
       String rd = a["start_date_local"].as<String>();
       if(rd.length() >= 10) latestStrava.date = rd.substring(8,10)+"."+rd.substring(5,7)+".";

       // Pace / Speed Berechnung
       if(latestStrava.type == "Run" && d > 0) {
          float p = (float)s / 60.0 / (d/1000.0);
          int pm = (int)p; int ps = (int)((p - pm) * 60);
          char buf[16]; sprintf(buf, "%d:%02d /km", pm, ps);
          latestStrava.paceOrSpeed = String(buf);
       } else if (d > 0) {
          float kmh = (d/1000.0) / ((float)s/3600.0);
          latestStrava.paceOrSpeed = String(kmh, 1) + " km/h";
       }
       latestStrava.hasData = true;
    }
  }
  http.end(); 

  // -----------------------------
  // TEIL B: Statistiken holen
  // -----------------------------
  String statsUrl = "https://www.strava.com/api/v3/athletes/" + STRAVA_ATHLETE_ID + "/stats";
  http.begin(client, statsUrl);
  http.addHeader("Authorization", "Bearer " + token); // Gleicher Token!
  int codeStats = http.GET();
  
  if (codeStats == 200) {
     DynamicJsonDocument docS(2048);
     deserializeJson(docS, http.getString());
     
     // Recent (4 Wochen)
     stravaStats.recentRunDist = docS["recent_run_totals"]["distance"].as<float>() / 1000.0;
     stravaStats.recentRunCount = docS["recent_run_totals"]["count"];
     
     stravaStats.recentRideDist = docS["recent_ride_totals"]["distance"].as<float>() / 1000.0;
     stravaStats.recentRideCount = docS["recent_ride_totals"]["count"];
     
     // YTD (Jahr)
     stravaStats.yearRunDist = docS["ytd_run_totals"]["distance"].as<float>() / 1000.0;
     stravaStats.yearRideDist = docS["ytd_ride_totals"]["distance"].as<float>() / 1000.0;
     
     stravaStats.hasData = true;
  }
  http.end();
  
  Serial1.println("Strava Combined - Fertig.");
}
bool isDeviceOnline() {
  Serial1.print("Pinge Gerät: " + MONITOR_IP + " ... ");
  
  // Wir machen einen IPAddress Typ aus dem String
  IPAddress ip;
  ip.fromString(MONITOR_IP);
  
  // Ping mit kurzem Timeout 
  bool success = Ping.ping(ip, 4);
  
  if (success) {
    Serial1.println("Online!");
  } else {
    Serial1.println("Offline / Nicht erreichbar.");
  }
  return success;
}