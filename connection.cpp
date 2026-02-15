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
  Serial1.println("Tagesschau - Datenabruf (PSRAM Mode) - Beginn");

  WiFiClientSecure client;
  client.setInsecure(); // Zertifikate ignorieren spart Rechenzeit
  HTTPClient http;

  String url = "https://www.tagesschau.de/api2u/homepage";
  
  http.begin(client, url);
  http.setTimeout(20000); // 20 Sekunden Timeout
  
  int httpCode = http.GET();
  
  Serial1.print("HTTP Status: ");
  Serial1.println(httpCode);

  if (httpCode == 200) {
    // SCHRITT 1: Alles in einen String laden.
    // Der ESP32S3 legt große Strings automatisch im PSRAM ab, wenn aktiviert.
    Serial1.println("Lade JSON... bitte warten...");
    String payload = http.getString();
    
    Serial1.print("Download fertig. Größe: ");
    Serial1.print(payload.length());
    Serial1.println(" Bytes");

    if (payload.length() > 0) {
        DynamicJsonDocument doc(45000); 

        // Filter definieren
        DynamicJsonDocument filter(512);
        filter["news"][0]["title"] = true;
        filter["news"][0]["date"] = true;
        filter["news"][0]["firstSentence"] = true;

        Serial1.println("Starte Deserialisierung...");
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

        if (!error) {
            JsonArray news = doc["news"];
            int availableNews = news.size();
            Serial1.print("Gefundene News-Elemente: ");
            Serial1.println(availableNews);

            newsCount = 0;
            for (JsonObject n : news) {
                if (newsCount >= 5) break;
                
                String rawTitle = n["title"].as<String>();
                String rawDate = n["date"].as<String>();
                
                Serial1.print("News "); Serial1.print(newsCount+1); Serial1.print(": ");
                Serial1.println(rawTitle);
                
                // ... hier dein Zuweisungscode für newsList ...
                newsList[newsCount].headline = cleanText(rawTitle);
                 if(rawDate.length() > 16) {
                     newsList[newsCount].time = rawDate.substring(11, 16);
                 } else {
                     newsList[newsCount].time = "--:--";
                 }

                newsCount++;
            }
        } else {
            Serial1.print("JSON Parsing Fehler: ");
            Serial1.println(error.c_str());
        }
    } else {
        Serial1.println("Fehler: Leere Antwort erhalten!");
    }
  } else {
    Serial1.println("HTTP Verbindung fehlgeschlagen");
  }
  http.end();
  Serial1.println("Tagesschau - Datenabruf - Ende");
}

time_t timegm_impl(struct tm *tm) {
  time_t t = mktime(tm);
  return t + 3600; // Grobe Korrektur, sauberer ist unten die Offset-Berechnung
}

// --- UNTERFUNKTION: Formel 1 ---
bool fetchF1Data(time_t now) {
  Serial1.println("SPORT: Lade F1...");
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  
  String url = "https://api.jolpi.ca/ergast/f1/current/next.json";
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  bool f1Active = false;
  nextF1.hasRace = false; // Reset

  if (httpCode == 200) {
    Stream& stream = http.getStream();
    
    DynamicJsonDocument doc(4096); 
    DeserializationError error = deserializeJson(doc, stream); 

    if (!error) {
      JsonObject race = doc["MRData"]["RaceTable"]["Races"][0];
      if (!race.isNull()) {
        nextF1.raceName = cleanText(race["raceName"].as<String>());
        if (race.containsKey("Circuit")) {
          nextF1.circuit = cleanText(race["Circuit"]["circuitName"].as<String>());
        } 
        String dateStr = race["date"].as<String>(); // "2025-03-16"
        String timeStr = race["time"].as<String>(); // "14:00:00Z"
        
        struct tm tm_utc = {0};
        strptime(dateStr.c_str(), "%Y-%m-%d", &tm_utc);
        int h, m, s;
        sscanf(timeStr.c_str(), "%d:%d:%d", &h, &m, &s);
        tm_utc.tm_hour = h; tm_utc.tm_min = m; tm_utc.tm_sec = s;
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        long offsetSec = 0;
        if (timeinfo.tm_isdst == 1) offsetSec = 7200; // Sommerzeit +2h
        else offsetSec = 3600; // Winterzeit +1h

        time_t raceTimeRaw = mktime(&tm_utc); 
        time_t raceTimeLocal = raceTimeRaw; //
        int localHour = (h + (timeinfo.tm_isdst ? 2 : 1)) % 24;
        
        // String bauen
        char timeBuf[16];
        sprintf(timeBuf, "%02d:%02d", localHour, m);
        
        // Datum formatieren
        String dStr = dateStr.substring(8, 10);
        String mStr = dateStr.substring(5, 7);
        nextF1.raceTime = "Rennen: " + dStr + "." + mStr + ". " + String(timeBuf) + " Uhr";

        if (race.containsKey("Qualifying")) {
           String qTime = race["Qualifying"]["time"].as<String>();
           int qH = qTime.substring(0,2).toInt();
           int qLocalH = (qH + (timeinfo.tm_isdst ? 2 : 1)) % 24;
           char qBuf[10];
           sprintf(qBuf, "%02d:%s", qLocalH, qTime.substring(3,5).c_str());
           nextF1.qualiTime = "Quali: " + String(qBuf) + " Uhr";
        } else {
           nextF1.qualiTime = "Quali: --:--";
        }
        nextF1.hasRace = true;
     }
    }
  }
  http.end();
  return f1Active;
}

// --- UNTERFUNKTION: Fußball (BL1 oder UCL) ---
bool fetchOpenLigaDB(String league, time_t now) {
  Serial1.print("SPORT: Lade Liga: "); Serial1.println(league);
  
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://api.openligadb.de/getmatchdata/" + league;
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  bool matchFound = false;
  blMatchCount = 0; // Reset (nutzen wir auch für UCL)

  if (httpCode == 200) {
    String payload = http.getString();
    Serial1.printf("Payload Größe: %d bytes\n", payload.length()); 
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
      
      if (arr.size() > 0) {
         String firstDate = arr[0]["matchDateTime"].as<String>();
         struct tm tm_m = {0};
         strptime(firstDate.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_m);
         time_t matchTime = mktime(&tm_m);
         double diff = difftime(matchTime, now);
         matchFound = true;
      }

      for (JsonObject match : arr) {
        if (blMatchCount >= 9) break;

        String t1 = cleanText(match["team1"]["shortName"].as<String>());
        String t2 = cleanText(match["team2"]["shortName"].as<String>());
        
        // Wenn Teams leer sind (passiert bei UCL manchmal bei "TBD"), überspringen
        if(t1 == "null" || t2 == "null") continue;

        blMatches[blMatchCount].homeTeam = t1;
        blMatches[blMatchCount].guestTeam = t2;

        if (match["matchIsFinished"]) {
          int s1 = 0, s2 = 0;
          for (JsonObject res : match["matchResults"].as<JsonArray>()) {
            if (res["resultTypeID"] == 2) {
               s1 = res["pointsTeam1"];
               s2 = res["pointsTeam2"];
               break;
            }
          }
          blMatches[blMatchCount].timeString = String(s1) + ":" + String(s2);
        } else {
          String dt = match["matchDateTime"].as<String>();
          struct tm tm_time = {0};
          strptime(dt.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_time);
          char buf[16];
          strftime(buf, sizeof(buf), "%a %H:%M", &tm_time);
          String sDate = String(buf);
          sDate.replace("Fri", "Fr"); sDate.replace("Sat", "Sa"); sDate.replace("Sun", "So");
          sDate.replace("Mon", "Mo"); sDate.replace("Tue", "Di"); sDate.replace("Wed", "Mi"); sDate.replace("Thu", "Do");
          blMatches[blMatchCount].timeString = sDate;
        }
        blMatchCount++;
      }
    } else {
        Serial1.print("SPORT: JSON Error "); Serial1.println(error.c_str());
    }
  }
  http.end();
  return matchFound;
}
void fetchSportsData() {
  Serial1.println("--------------------------------");
  Serial1.println("SPORT: Datenabruf Start");

  if (WiFi.status() != WL_CONNECTED) {
    Serial1.println("SPORT: WiFi nicht verbunden!");
    return;
  }
  time_t now;
  time(&now);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial1.println("SPORT: Konnte Zeit nicht holen.");
    return;
  }
  
  // Flag zurücksetzen
  bool eventFound = false;

  // ------------------------------
  // TEIL A: Formel 1 abrufen
  // ------------------------------
  if (fetchF1Data(now)) {
    eventFound = true;
  }

  // ------------------------------
  // TEIL B: Fußball Logik (BL vs UCL)
  // ------------------------------
  int wday = timeinfo.tm_wday;
  String leagueShortcut = "bl1"; // Standard
  if (wday == 2 || wday == 3) {
    Serial1.println("SPORT: Es ist Di/Mi -> Prüfe Champions League");
    leagueShortcut = "ucl";
  } else {
    Serial1.println("SPORT: Es ist Do-Mo -> Prüfe Bundesliga");
  }

  if (fetchOpenLigaDB(leagueShortcut, now)) {
    eventFound = true;
  }

  // Globalen Status setzen
  isSportWeekend = eventFound;
  cachedSportWeekend = eventFound;
  Serial1.println("SPORT: Datenabruf Ende");
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
    filter[0]["name"] = true; 
    filter[0]["distance"] = true; 
    filter[0]["moving_time"] = true; 
    filter[0]["type"] = true;
    filter[0]["start_date_local"] = true;
    filter[0]["map"]["summary_polyline"] = true; // Die GPS Daten
    
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    
    if (doc.size() > 0) {
       JsonObject a = doc[0];
       latestStrava.name = cleanText(a["name"].as<String>());
       latestStrava.type = a["type"].as<String>();
       Serial1.print("Strava Type: ");
       Serial1.println(latestStrava.type);
       latestStrava.polyline = a["map"]["summary_polyline"].as<String>();
       Serial1.print("Strava Polyline Length: ");
       Serial1.println(latestStrava.polyline.length());
       Serial1.print("Free Heap after Strava: ");
       Serial1.println(ESP.getFreeHeap());
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