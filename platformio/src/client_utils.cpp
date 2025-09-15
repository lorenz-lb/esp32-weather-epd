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
#include "HardwareSerial.h"
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

  

/* Perform an HTTP GET request to OpenWeatherMap's "One Call" API
 * If data is received, it will be parsed and stored in the global variable
 * owm_onecall.
 *
 * Returns the HTTP Status Code.
 */
int getDWDonecall(WiFiClientSecure &client, dwd_resp_onecall_t &r, tm &time_info)
{

  tm endDate = time_info;
  addDays(endDate,5);

  char startTimeBuffer[12];
  char endTimeBuffer[12];
  strftime(startTimeBuffer, sizeof(startTimeBuffer), "%Y-%m-%d", &time_info);
  strftime(endTimeBuffer, sizeof(endTimeBuffer), "%Y-%m-%d", &endDate);

  int attempts = 0;
  bool rxSuccess = false;
  DeserializationError jsonErr = {};
  String uri = "/weather?lat=" + LAT +
               "&lon=" + LON + "&date=" + startTimeBuffer + "&last_date=" + endTimeBuffer;

  Serial.println("***** " + OWM_ENDPOINT + ":" + OWM_PORT + uri);

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
      Serial.println("start deserialization");
      jsonErr = deserializeOneCall(http.getStream(), r, time_info);
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
} 

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

void addDays(tm& time_info, int days) {
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // leap year
    int current_year = time_info.tm_year + 1900;
    if ((current_year % 4 == 0 && current_year % 100 != 0) || (current_year % 400 == 0)) {
        days_in_month[1] = 29; // February has 29 days in a leap year
    }
    
    time_info.tm_mday += days;

    // rollover
    while (time_info.tm_mday > days_in_month[time_info.tm_mon]) {
        time_info.tm_mday -= days_in_month[time_info.tm_mon];
        time_info.tm_mon++;
        
        if (time_info.tm_mon > 11) {
            time_info.tm_mon = 0;
            time_info.tm_year++;
            
            current_year = time_info.tm_year + 1900;
            if ((current_year % 4 == 0 && current_year % 100 != 0) || (current_year % 400 == 0)) {
                days_in_month[1] = 29;
            } else {
                days_in_month[1] = 28;
            }
        }
    }
}