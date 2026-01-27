#ifndef SECRETS_H
#define SECRETS_H

static const char* WIFI_SSID     = "WLAN SSID";      
static const char* WIFI_PASSWORD = "WLAN PASSWORT"; 
static const String GOOGLE_API_KEY = "Google Cloud API Key für Google Maps"; 
// ... (unterhalb deiner anderen Konfigurationen)

// STRAVA KONFIGURATION
// Ersetze dies mit deinen echten Daten
const String STRAVA_CLIENT_ID     = "Strava Client ID (API)"; 
const String STRAVA_CLIENT_SECRET = "Strava Client Secret (API)";
const String STRAVA_ATHLETE_ID = "Strava Athlet ID (API)";
const String STRAVA_REFRESH_TOKEN = "Strava API Refresh Token";

// GEO & ROUTING (Verschoben aus settings.h)
const String ORIGIN       = "Musterstraße 1, Berlin"; 
const String DESTINATION  = "Zielstraße 2, Potsdam"; 
const String LAT = "52.52";
const String LON = "13.40";

// ÖPNV
const String STOP_ID      = "900000001"; // Beispiel ID

//IP-Adressenkonfiguration
const IPAddress local_IP(192, 168, 1, 1); 
const IPAddress gateway(192, 168, 178, 1);    
const IPAddress subnet(255, 255, 255, 0);     
const IPAddress primaryDNS(8, 8, 8, 8);       

// PRESENCE DETECTION
const String MONITOR_IP = "XXX.XXX.XXX.XXX"; // Ersetze XXX mit der IP deines Laptops/Handys
#endif