/* Client side utilities for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// built-in C++ libraries
#include <cstring>
#include <vector>

// arduino/esp32 libraries
#include <Arduino.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>


// additional libraries
#include <Adafruit_BusIO_Register.h>
#include <ArduinoJson.h>

// header files
#include "_locale.h"
#include "api_response.h"
#include "aqi.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "renderer.h"
#ifndef USE_HTTP
#include <WiFiClientSecure.h>
#endif

#ifdef USE_HTTP
static const uint16_t OWM_PORT = 80;
#else
static const uint16_t OWM_PORT = 443;
#endif

/* Power-on and connect WiFi.
 * Takes int parameter to store WiFi RSSI, or “Received Signal Strength
 * Indicator"
 *
 * Returns WiFi status.
 */
wl_status_t startWiFi(int &wifiRSSI) {
  WiFi.mode(WIFI_STA);
  Serial.printf("%s '%s'", TXT_CONNECTING_TO, WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // timeout if WiFi does not connect in WIFI_TIMEOUT ms from now
  unsigned long timeout = millis() + WIFI_TIMEOUT;
  wl_status_t connection_status = WiFi.status();

  while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
    Serial.print(".");
    delay(50);
    connection_status = WiFi.status();
  }
  Serial.println();

  if (connection_status == WL_CONNECTED) {
    wifiRSSI = WiFi.RSSI(); // get WiFi signal strength now, because the WiFi
                            // will be turned off to save power!
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    Serial.printf("%s '%s'\n", TXT_COULD_NOT_CONNECT_TO, WIFI_SSID);
  }
  return connection_status;
} // startWiFi

/* Disconnect and power-off WiFi.
 */
void killWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
} // killWiFi

/* Prints the local time to serial monitor.
 *
 * Returns true if getting local time was a success, otherwise false.
 */
bool printLocalTime(tm *timeInfo) {
  int attempts = 0;
  while (!getLocalTime(timeInfo) && attempts++ < 3) {
    Serial.println(TXT_FAILED_TO_GET_TIME);
    return false;
  }
  Serial.println(timeInfo, "%A, %B %d, %Y %H:%M:%S");
  return true;
} // printLocalTime

/* Waits for NTP server time sync, adjusted for the time zone specified in
 * config.cpp.
 *
 * Returns true if time was set successfully, otherwise false.
 *
 * Note: Must be connected to WiFi to get time from NTP server.
 */
bool waitForSNTPSync(tm *timeInfo) {
  // Wait for SNTP synchronization to complete
  unsigned long timeout = millis() + NTP_TIMEOUT;
  if ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) &&
      (millis() < timeout)) {
    Serial.print(TXT_WAITING_FOR_SNTP);
    delay(100); // ms
    while ((sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) &&
           (millis() < timeout)) {
      Serial.print(".");
      delay(100); // ms
    }
    Serial.println();
  }
  return printLocalTime(timeInfo);
} // waitForSNTPSync

void mockWeatherCall(owm_resp_onecall_t &r) {
  r.lat = 0.0;
  r.lon = 0.0;
  r.timezone = "test";
  r.timezone_offset = 1;
  // Einfache Standardwerte für ein Mock-Objekt
  r.current.dt = 1672531200; // Beispiel: Unix-Zeitstempel
  r.current.sunrise = 1672520400;
  r.current.sunset = 1672558800;
  r.current.temp = 15.5f;
  r.current.feels_like = 14.2f;
  r.current.pressure = 1012;
  r.current.humidity = 75;
  r.current.dew_point = 10.1f;
  r.current.clouds = 40;
  r.current.uvi = 3.5f;
  r.current.visibility = 10000; // Meter
  r.current.wind_speed = 12.3f;
  r.current.wind_gust = 18.7f;
  r.current.wind_deg = 270;
  r.current.rain_1h = 0.0f;
  r.current.snow_1h = 0.0f;

  r.current.weather.id = 800; // Beispiel: Clear sky (OpenWeatherMap)
  r.current.weather.main = "Clear";
  r.current.weather.description = "clear sky";
  r.current.weather.icon = "01d";


// Angenommen, OWM_NUM_HOURLY ist definiert (z.B. 48 für 48 Stunden)
int64_t initial_dt = 1672531200; // Beispiel: Unix-Zeitstempel für Mitternacht

for (int i = 0; i < OWM_NUM_HOURLY; ++i) {
    // Einfache Simulation der Tageszeit: dt erhöht sich um 3600 Sekunden pro Stunde
    r.hourly[i].dt = initial_dt + (i * 3600);
    
    // Temperatur steigt und fällt im Tagesverlauf
    float temp_base = 10.0f;
    float temp_variation = 10.0f; 
    r.hourly[i].temp = temp_base + temp_variation;
    
    r.hourly[i].feels_like = r.hourly[i].temp - 1.0f; // Etwas kühler
    r.hourly[i].pressure = 1012;
    r.hourly[i].humidity = 75;
    r.hourly[i].dew_point = 8.5f;
    r.hourly[i].clouds = 40;
    r.hourly[i].uvi = 3.5f;
    r.hourly[i].visibility = 10000;
    r.hourly[i].wind_speed = 5.0f;
    r.hourly[i].wind_gust = 8.0f;
    r.hourly[i].wind_deg = 270;
    r.hourly[i].pop = 0.1f; // Geringe Regenwahrscheinlichkeit
    r.hourly[i].rain_1h = 0.0f;
    r.hourly[i].snow_1h = 0.0f;

    // Wetterbeschreibung
    r.hourly[i].weather.id = 800;
    r.hourly[i].weather.main = "Clear";
    r.hourly[i].weather.description = "clear sky";
    r.hourly[i].weather.icon = "01d";
}



for (int i = 0; i < OWM_NUM_DAILY; ++i) {
  r.daily[i].temp = (owm_temp_t){10.0f, 22.5f, 25.0f, 5.0f,25.0f,5.0f};
  r.daily[i].clouds = 20;
  r.daily[i].weather.id = 800;
  r.daily[i].weather.icon = "01d";
  r.daily[i].weather.main = "Rain";
  r.daily[i].weather.description= "light rain";
}


}

void mockPollutionCall(owm_resp_air_pollution_t &r) {
// Annahme: OWM_NUM_AIR_POLLUTION ist definiert
int64_t initial_dt_air = 1672531200; // Beispiel: Unix-Zeitstempel

for (int i = 0; i < OWM_NUM_AIR_POLLUTION; ++i) {
    r.main_aqi[i] = 1; // Air Quality Index: 1 = Good
    
    // Konzentrationen der Schadstoffe
    r.components.co[i] = 200.0f; // CO in µg/m³
    r.components.no[i] = 0.5f;   // NO in µg/m³
    r.components.no2[i] = 10.0f;  // NO₂ in µg/m³
    r.components.o3[i] = 50.0f;   // O₃ in µg/m³
    r.components.so2[i] = 5.0f;   // SO₂ in µg/m³
    r.components.pm2_5[i] = 8.0f;  // PM2.5 in µg/m³
    r.components.pm10[i] = 15.0f;  // PM10 in µg/m³
    r.components.nh3[i] = 1.0f;   // NH₃ in µg/m³

    r.dt[i] = initial_dt_air + (i * 3600); // Zeitstempel für jede Stunde

    // Die "if"-Bedingung ist hier wie bei den anderen Schleifen nicht notwendig,
    // da die for-Schleife bis zur Array-Größe läuft.
}
}

/* Perform an HTTP GET request to OpenWeatherMap's "One Call" API
 * If data is received, it will be parsed and stored in the global variable
 * owm_onecall.
 *
 * Returns the HTTP Status Code.
 */
#ifdef USE_HTTP
int getOWMonecall(WiFiClient &client, owm_resp_onecall_t &r)
#else
int getOWMonecall(WiFiClientSecure &client, owm_resp_onecall_t &r)
#endif
{

  // TODO: mock data and return mock
  mockWeatherCall(r);
  return 200;

  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};
  String uri = "/data/" + OWM_ONECALL_VERSION + "/onecall?lat=" + LAT +
               "&lon=" + LON + "&lang=" + OWM_LANG +
               "&units=standard&exclude=minutely";
#if !DISPLAY_ALERTS
  // exclude alerts
  uri += ",alerts";
#endif

  // This string is printed to terminal to help with debugging. The API key is
  // censored to reduce the risk of users exposing their key.
  String sanitizedUri = OWM_ENDPOINT + uri + "&appid={API key}";

  uri += "&appid=" + OWM_APIKEY;

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + sanitizedUri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3) {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED) {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT); // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);        // default 5000ms
    http.begin(client, OWM_ENDPOINT, OWM_PORT, uri);
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      jsonErr = deserializeOneCall(http.getStream(), r);
      if (jsonErr) {
        // -256 offset distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " " +
                   getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
} // getOWMonecall

/* Perform an HTTP GET request to OpenWeatherMap's "Air Pollution" API
 * If data is received, it will be parsed and stored in the global variable
 * owm_air_pollution.
 *
 * Returns the HTTP Status Code.
 */
#ifdef USE_HTTP
int getOWMairpollution(WiFiClient &client, owm_resp_air_pollution_t &r)
#else
int getOWMairpollution(WiFiClientSecure &client, owm_resp_air_pollution_t &r)
#endif
{
  mockPollutionCall(r);
  return 200;

  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};

  // set start and end to appropriate values so that the last 24 hours of air
  // pollution history is returned. Unix, UTC.
  time_t now;
  int64_t end = time(&now);
  // minus 1 is important here, otherwise we could get an extra hour of history
  int64_t start = end - ((3600 * OWM_NUM_AIR_POLLUTION) - 1);
  char endStr[22];
  char startStr[22];
  sprintf(endStr, "%lld", end);
  sprintf(startStr, "%lld", start);
  String uri = "/data/2.5/air_pollution/history?lat=" + LAT + "&lon=" + LON +
               "&start=" + startStr + "&end=" + endStr + "&appid=" + OWM_APIKEY;
  // This string is printed to terminal to help with debugging. The API key is
  // censored to reduce the risk of users exposing their key.
  String sanitizedUri = OWM_ENDPOINT +
                        "/data/2.5/air_pollution/history?lat=" + LAT +
                        "&lon=" + LON + "&start=" + startStr +
                        "&end=" + endStr + "&appid={API key}";

  Serial.print(TXT_ATTEMPTING_HTTP_REQ);
  Serial.println(": " + sanitizedUri);
  int httpResponse = 0;
  while (!rxSuccess && attempts < 3) {
    wl_status_t connection_status = WiFi.status();
    if (connection_status != WL_CONNECTED) {
      // -512 offset distinguishes these errors from httpClient errors
      return -512 - static_cast<int>(connection_status);
    }

    HTTPClient http;
    http.setConnectTimeout(HTTP_CLIENT_TCP_TIMEOUT); // default 5000ms
    http.setTimeout(HTTP_CLIENT_TCP_TIMEOUT);        // default 5000ms
    http.begin(client, OWM_ENDPOINT, OWM_PORT, uri);
    httpResponse = http.GET();
    if (httpResponse == HTTP_CODE_OK) {
      jsonErr = deserializeAirQuality(http.getStream(), r);
      if (jsonErr) {
        // -256 offset to distinguishes these errors from httpClient errors
        httpResponse = -256 - static_cast<int>(jsonErr.code());
      }
      rxSuccess = !jsonErr;
    }
    client.stop();
    http.end();
    Serial.println("  " + String(httpResponse, DEC) + " " +
                   getHttpResponsePhrase(httpResponse));
    ++attempts;
  }

  return httpResponse;
} // getOWMairpollution

/* Prints debug information about heap usage.
 */
void printHeapUsage() {
  Serial.println("[debug] Heap Size       : " + String(ESP.getHeapSize()) +
                 " B");
  Serial.println("[debug] Available Heap  : " + String(ESP.getFreeHeap()) +
                 " B");
  Serial.println("[debug] Min Free Heap   : " + String(ESP.getMinFreeHeap()) +
                 " B");
  Serial.println("[debug] Max Allocatable : " + String(ESP.getMaxAllocHeap()) +
                 " B");
  return;
}
