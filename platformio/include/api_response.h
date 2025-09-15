/* API response deserialization declarations for esp32-weather-epd.
 * Copyright (C) 2022-2023  Luke Marzen
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

#ifndef __API_RESPONSE_H__
#define __API_RESPONSE_H__

#include "api_response.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cstdint>
#include <vector>


#define OWM_NUM_MINUTELY 1 // 61
#define OWM_NUM_HOURLY 24  // 48
#define OWM_NUM_DAILY 5    // 8
#define OWM_NUM_ALERTS 8 

#define DWD_NUM_DAILY 24
#define DWD_DAYS 5

#define WEATHER_CONDITIONS_SIZE 13

typedef enum weather_conditions {
  CLEAR_DAY = 0,
  CLEAR_NIGHT = 1,
  PARTLY_CLOUDY_DAY = 2,
  PARTLY_CLOUDY_NIGHT = 3,
  CLOUDY = 4,
  FOG = 5,
  WIND = 6,
  RAIN = 7 , 
  SLEET = 8,
  SNOW = 9 ,
  HAIL = 10,
  THUNDERSTORM = 11,
  UNNOWN = 12
} weather_conditions_t;

// ################### DWD ##################
typedef struct dwd_hourly {
  tm time;
  float precipitation;
  float pressure_msl;
  float sunshine;
  float temperatur;
  int wind_direction;
  float wind_speed;
  float wind_gust_speed;
  int cloud_cover;
  float dew_point;
  int relative_humidity;
  int visibility;
  String condition;
  int precipitation_probability;
  int precipitation_probability_6h;
  float solar;
  weather_conditions_t icon;

} dwd_hourly_t;

typedef struct dwd_current {
  dwd_hourly_t condition;
} dwd_current_t;

typedef struct dwd_daily {
  tm time;
  weather_conditions_t icon;
  float temp_max;
  float temp_min;
  float pop;
  float snow;
  float rain;
  float clouds;
  float wind_speed;
  float wind_gust;
} dwd_daily_t;

typedef struct dwd_resp_onecall {
  dwd_current current;
  dwd_hourly_t hours[DWD_NUM_DAILY * DWD_DAYS];
  dwd_daily_t days[DWD_DAYS];
} dwd_resp_onecall_t;

weather_conditions_t iconToEnum(String icon);
DeserializationError deserializeOneCall(WiFiClient &json,
                                        dwd_resp_onecall_t &r,
                                        tm &time_info);

#endif
