/* API response deserialization for esp32-weather-epd (DWD).
 * Copyright (C) 2025  Lorenz Braun
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

#include "api_response.h"
#include "ArduinoJson/Array/JsonArray.hpp"
#include "ArduinoJson/Document/JsonDocument.hpp"
#include "HardwareSerial.h"
#include "config.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <float.h>
#include <iterator>
#include <limits.h>
#include <time.h>
#include <vector>

weather_conditions_t iconToEnum(String icon) {
  if (icon == "clear-day") {
    return CLEAR_DAY;
  }
  if (icon == "clear-night") {
    return CLEAR_NIGHT;
  }
  if (icon == "partly-cloudy-day") {
    return PARTLY_CLOUDY_DAY;
  }
  if (icon == "partly-cloudy-night") {
    return PARTLY_CLOUDY_NIGHT;
  }
  if (icon == "cloudy") {
    return CLOUDY;
  }
  if (icon == "fog") {
    return FOG;
  }
  if (icon == "wind") {
    return WIND;
  }
  if (icon == "rain") {
    return RAIN;
  }
  if (icon == "sleet") {
    return SLEET;
  }
  if (icon == "snow") {
    return SNOW;
  }
  if (icon == "hail") {
    return HAIL;
  }
  if (icon == "thunderstorm") {
    return THUNDERSTORM;
  }

  return UNNOWN;
}

int encodeIcon(String icon) {

  int num = 0;

  if (icon == "thunderstorm") {
    num = 200;
  } else if (icon == "sleet") {
    num = 600;
  } else if (icon == "fog") {
    num = 741;
  } else if (icon == "clear-day") {
    num = 800;
  } else if (icon == "partly-cloud-day") {
    num = 802;
  } else if (icon == "cloudy") {
    num = 804;
  }

  return num;
}

void printTime(tm &timeInfo) {
  Serial.printf("Time: %i-%i-%iT%i:%i\n", timeInfo.tm_year + 1900,
                timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour,
                timeInfo.tm_min);
}

// time like 2025-08-10T00:00:00+00:00
void parseTime(String timeString, tm &tm_info) {

  int year, month, day, hour, minute, second;
  sscanf(timeString.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour,
         &minute, &second);

  tm_info.tm_year = year - 1900;
  tm_info.tm_mday = day;
  tm_info.tm_hour = hour;
  tm_info.tm_min = minute;
  tm_info.tm_sec = second;
  // index based
  // tm_info.tm_mon = month;
  tm_info.tm_mon = month - 1;
}

DeserializationError deserializeOneCall(WiFiClient &json, dwd_resp_onecall_t &r,
                                        tm &current_time) {

  int i;

  // ############## FILTER DOCUMENT ##############
  JsonDocument filter;
  filter["weather"][0]["timestamp"] = true;
  filter["weather"][0]["precipitation"] = true;
  filter["weather"][0]["pressure_msl"] = true;
  filter["weather"][0]["sunshine"] = true;
  filter["weather"][0]["temperature"] = true;
  filter["weather"][0]["wind_direction"] = true;
  filter["weather"][0]["wind_speed"] = true;
  filter["weather"][0]["cloud_cover"] = true;
  filter["weather"][0]["dew_point"] = true;
  filter["weather"][0]["relative_humidity"] = true;
  filter["weather"][0]["visibility"] = true;
  filter["weather"][0]["condition"] = true;
  filter["weather"][0]["precipitation_probability"] = true;
  filter["weather"][0]["precipitation_probability_6h"] = true;
  filter["weather"][0]["solar"] = true;
  filter["weather"][0]["icon"] = true;

  filter["sources"] = false;

  JsonDocument doc;
  DeserializationError error =
      deserializeJson(doc, json, DeserializationOption::Filter(filter));

  if (error) {
    return error;
  }

  // ############## extract data from document ##############
  i = 0;
  tm prev_time = {};

  int daily_icon[WEATHER_CONDITIONS_SIZE] = {0};

  prev_time.tm_mday = current_time.tm_mday;
  float min_temp = FLT_MAX;
  float max_temp = FLT_MIN;
  int idx_day = 0;
  float hour_temperature = FLT_MAX;
  float hour_precipitation = 0.0f;
  float sum_precipitation = 0.0f;

  for (JsonObject hourly : doc["weather"].as<JsonArray>()) {
    struct tm tm_info = {};
    const char *time_string = hourly["timestamp"].as<const char *>();
    parseTime(time_string, tm_info);
    hour_precipitation = hourly["precipitation"].as<float>();
    hour_temperature = hourly["temperature"].as<float>();
    r.hours[i].time = tm_info;

    r.hours[i].precipitation = hour_precipitation;
    r.hours[i].pressure_msl = hourly["pressure_msl"].as<float>();
    r.hours[i].sunshine = hourly["sunshine"].as<float>();
    r.hours[i].temperatur = hour_temperature;
    r.hours[i].wind_direction = hourly["wind_direction"].as<int>();
    r.hours[i].wind_speed = hourly["wind_speed"].as<float>();
    r.hours[i].wind_gust_speed = hourly["wind_gust_speed"].as<float>();
    r.hours[i].cloud_cover = hourly["cloud_cover"].as<int>();
    r.hours[i].dew_point = hourly["dew_point"].as<float>();
    r.hours[i].relative_humidity = hourly["relative_humidity"].as<int>();
    r.hours[i].visibility = hourly["visibility"].as<int>();
    r.hours[i].condition = hourly["condition"].as<String>();
    r.hours[i].precipitation_probability =
        hourly["precipitation_probability"].as<int>();
    r.hours[i].precipitation_probability_6h =
        hourly["precipitation_probability_6h"].as<int>();
    r.hours[i].solar = hourly["solar"].as<float>();

    r.hours[i].icon = iconToEnum(hourly["icon"].as<const char *>());

    // check new day
    if (prev_time.tm_mday != tm_info.tm_mday ||
        (i == DWD_NUM_DAILY * DWD_DAYS - 1)) {
      // set day data
      // min max temp
      r.days[idx_day].temp_max = std::max(max_temp, hour_temperature);
      r.days[idx_day].temp_min = std::min(min_temp, hour_temperature);

      // rain sum
      r.days[idx_day].rain = sum_precipitation;

      // set day time
      prev_time.tm_hour = 0;
      r.days[idx_day].time = prev_time;

      // TODO ! Icon
      int *maxElementPtr =
          std::max_element(daily_icon, daily_icon + WEATHER_CONDITIONS_SIZE);
      int ic = std::distance(daily_icon, maxElementPtr);

      r.days[idx_day].icon = static_cast<weather_conditions_t>(ic);

      // debug
      for (int i = 0; i < WEATHER_CONDITIONS_SIZE; ++i) {
        Serial.printf("%d: %d \n",i,daily_icon[i]);
      }

      // reset accumolator
      sum_precipitation = 0.0f;
      min_temp = FLT_MAX;
      max_temp = FLT_MIN;
      std::fill(daily_icon, daily_icon + WEATHER_CONDITIONS_SIZE, 0);

      idx_day += 1;
    }

    // daily accumolator
    min_temp = std::min(min_temp, hour_temperature);
    max_temp = std::max(max_temp, hour_temperature);
    sum_precipitation += hour_precipitation;
    daily_icon[r.hours[i].icon]++;

    // current Weather, round to nearest hour
    if (current_time.tm_mday == tm_info.tm_mday) {
      if (((current_time.tm_min < 30) &&
           (current_time.tm_hour == tm_info.tm_hour)) ||
          ((current_time.tm_min >= 30) &&
           (current_time.tm_hour + 1 == tm_info.tm_hour))) {

        r.current.condition.time = r.hours[i].time;
        r.current.condition.precipitation = r.hours[i].precipitation;
        r.current.condition.pressure_msl = r.hours[i].pressure_msl;
        r.current.condition.sunshine = r.hours[i].sunshine;
        r.current.condition.temperatur = r.hours[i].temperatur;
        r.current.condition.wind_direction = r.hours[i].wind_direction;
        r.current.condition.wind_speed = r.hours[i].wind_speed;
        r.current.condition.wind_gust_speed = r.hours[i].wind_gust_speed;
        r.current.condition.cloud_cover = r.hours[i].cloud_cover;
        r.current.condition.dew_point = r.hours[i].dew_point;
        r.current.condition.relative_humidity = r.hours[i].relative_humidity;
        r.current.condition.visibility = r.hours[i].visibility;
        r.current.condition.condition = r.hours[i].condition;
        r.current.condition.precipitation_probability =
            r.hours[i].precipitation_probability;
        r.current.condition.precipitation_probability_6h =
            r.hours[i].precipitation_probability_6h;
        r.current.condition.solar = r.hours[i].solar;
        r.current.condition.icon = r.hours[i].icon;

        // debug
      }
    }

    if (i == DWD_NUM_DAILY * DWD_DAYS - 1) {
      break;
    }

    prev_time = tm_info;
    ++i;
  }

  return error;
}
