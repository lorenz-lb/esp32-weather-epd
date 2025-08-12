// /* API response deserialization for esp32-weather-epd (DWD).
 // * Copyright (C) 2025  Lorenz Braun
 // *
 // * This program is free software: you can redistribute it and/or modify
 // * it under the terms of the GNU General Public License as published by
 // * the Free Software Foundation, either version 3 of the License, or
 // * (at your option) any later version.
 // *
 // * This program is distributed in the hope that it will be useful,
 // * but WITHOUT ANY WARRANTY; without even the implied warranty of
 // * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 // * GNU General Public License for more details.
 // *
 // * You should have received a copy of the GNU General Public License
 // * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 // */


 
// #include <vector>
// #include <ArduinoJson.h>
// #include "api_response.h"
// #include "config.h"

// DeserializationError deserializeOneCall(WiFiClient &json,
                                        // dwd_resp_onecall_t &r)
// {
  // int i;

  // JsonDocument filter;
  // filter["weather"]  = true;
  // filter["sources"]  = false;

  // JsonDocument doc;
  // DeserializationError error = deserializeJson(doc, json,
                                         // DeserializationOption::Filter(filter));

  // if (error) {
    // return error;
  // }

  // i = 0;
  // for (JsonObject hourly : doc["weather"].as<JsonArray>())
  // {
    // // TODO: timestamp unix like
    // //r.hourly[i].dt         = hourly["dt"]        .as<int64_t>();
    // r.hourly[i].temp       = hourly["temperature"]      .as<float>();
    // //r.hourly[i].feels_like = hourly["feels_like"].as<float>();
    // r.hourly[i].pressure   = hourly["pressure_msl"]  .as<int>();
    // r.hourly[i].humidity   = hourly["relative_humidity"]  .as<int>();
    // r.hourly[i].dew_point  = hourly["dew_point"] .as<float>();
    // r.hourly[i].clouds     = hourly["cloud_cover"]    .as<int>();
    // //r.hourly[i].uvi        = hourly["uvi"]       .as<float>();
    // r.hourly[i].visibility = hourly["visibility"].as<int>();
    // r.hourly[i].wind_speed = hourly["wind_speed"].as<float>();
    // r.hourly[i].wind_gust  = hourly["wind_gust_speed"] .as<float>();
    // r.hourly[i].wind_deg   = hourly["wind_direction"]  .as<int>();

    // if (i == DWD_NUM_DAILY*DWD_NUM_DAILY - 1)
    // {
      // break;
    // }
    // ++i;
  // }

  // return error;
// } // end deserializeOneCall

// DeserializationError deserializeAirQuality(WiFiClient &json,
                                           // owm_resp_air_pollution_t &r)
// {
  // int i = 0;

  // JsonDocument doc;

  // DeserializationError error = deserializeJson(doc, json);
// #if DEBUG_LEVEL >= 1
  // Serial.println("[debug] doc.overflowed() : "
                 // + String(doc.overflowed()));
// #endif
// #if DEBUG_LEVEL >= 2
  // serializeJsonPretty(doc, Serial);
// #endif
  // if (error) {
    // return error;
  // }

  // r.coord.lat = doc["coord"]["lat"].as<float>();
  // r.coord.lon = doc["coord"]["lon"].as<float>();

  // for (JsonObject list : doc["list"].as<JsonArray>())
  // {

    // r.main_aqi[i] = list["main"]["aqi"].as<int>();

    // JsonObject list_components = list["components"];
    // r.components.co[i]    = list_components["co"].as<float>();
    // r.components.no[i]    = list_components["no"].as<float>();
    // r.components.no2[i]   = list_components["no2"].as<float>();
    // r.components.o3[i]    = list_components["o3"].as<float>();
    // r.components.so2[i]   = list_components["so2"].as<float>();
    // r.components.pm2_5[i] = list_components["pm2_5"].as<float>();
    // r.components.pm10[i]  = list_components["pm10"].as<float>();
    // r.components.nh3[i]   = list_components["nh3"].as<float>();

    // r.dt[i] = list["dt"].as<int64_t>();

    // if (i == OWM_NUM_AIR_POLLUTION - 1)
    // {
      // break;
    // }
    // ++i;
  // }

  // return error;
// } // end deserializeAirQuality

