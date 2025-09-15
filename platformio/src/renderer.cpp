/* Renderer for esp32-weather-epd.
 * Copyright (C) 2022-2025  Luke Marzen
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

#include "renderer.h"
#include "HardwareSerial.h"
#include "Print.h"
#include "_locale.h"
#include "_strftime.h"
#include "api_response.h"
#include "config.h"
#include "conversions.h"
#include "display_utils.h"

// fonts
#include FONT_HEADER

// icon header files
#include "icons/icons_128x128.h"
#include "icons/icons_160x160.h"
#include "icons/icons_16x16.h"
#include "icons/icons_196x196.h"
#include "icons/icons_24x24.h"
#include "icons/icons_32x32.h"
#include "icons/icons_48x48.h"
#include "icons/icons_64x64.h"
#include "icons/icons_96x96.h"

#ifdef DISP_BW_V2
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>
    display(GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#endif

#ifndef ACCENT_COLOR
#define ACCENT_COLOR GxEPD_BLACK
#endif

/* Returns the string width in pixels
 */
uint16_t getStringWidth(const String &text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

/* Returns the string height in pixels
 */
uint16_t getStringHeight(const String &text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return h;
}

/* Draws a string with alignment
 */
void drawString(int16_t x, int16_t y, const String &text, alignment_t alignment,
                uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextColor(color);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT) {
    x = x - w;
  }
  if (alignment == CENTER) {
    x = x - w / 2;
  }
  display.setCursor(x, y);
  display.print(text);
  return;
} // end drawString

/* Draws a string that will flow into the next line when max_width is reached.
 * If a string exceeds max_lines an ellipsis (...) will terminate the last word.
 * Lines will break at spaces(' ') and dashes('-').
 *
 * Note: max_width should be big enough to accommodate the largest word that
 *       will be displayed. If an unbroken string of characters longer than
 *       max_width exist in text, then the string will be printed beyond
 *       max_width.
 */
void drawMultiLnString(int16_t x, int16_t y, const String &text,
                       alignment_t alignment, uint16_t max_width,
                       uint16_t max_lines, int16_t line_spacing,
                       uint16_t color) {
  uint16_t current_line = 0;
  String textRemaining = text;
  // print until we reach max_lines or no more text remains
  while (current_line < max_lines && !textRemaining.isEmpty()) {
    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(textRemaining, 0, 0, &x1, &y1, &w, &h);

    int endIndex = textRemaining.length();
    // check if remaining text is to wide, if it is then print what we can
    String subStr = textRemaining;
    int splitAt = 0;
    int keepLastChar = 0;
    while (w > max_width && splitAt != -1) {
      if (keepLastChar) {
        // if we kept the last character during the last iteration of this while
        // loop, remove it now so we don't get stuck in an infinite loop.
        subStr.remove(subStr.length() - 1);
      }

      // find the last place in the string that we can break it.
      if (current_line < max_lines - 1) {
        splitAt = std::max(subStr.lastIndexOf(" "), subStr.lastIndexOf("-"));
      } else {
        // this is the last line, only break at spaces so we can add ellipsis
        splitAt = subStr.lastIndexOf(" ");
      }

      // if splitAt == -1 then there is an unbroken set of characters that is
      // longer than max_width. Otherwise if splitAt != -1 then we can continue
      // the loop until the string is <= max_width
      if (splitAt != -1) {
        endIndex = splitAt;
        subStr = subStr.substring(0, endIndex + 1);

        char lastChar = subStr.charAt(endIndex);
        if (lastChar == ' ') {
          // remove this char now so it is not counted towards line width
          keepLastChar = 0;
          subStr.remove(endIndex);
          --endIndex;
        } else if (lastChar == '-') {
          // this char will be printed on this line and removed next iteration
          keepLastChar = 1;
        }

        if (current_line < max_lines - 1) {
          // this is not the last line
          display.getTextBounds(subStr, 0, 0, &x1, &y1, &w, &h);
        } else {
          // this is the last line, we need to make sure there is space for
          // ellipsis
          display.getTextBounds(subStr + "...", 0, 0, &x1, &y1, &w, &h);
          if (w <= max_width) {
            // ellipsis fit, add them to subStr
            subStr = subStr + "...";
          }
        }

      } // end if (splitAt != -1)
    }   // end inner while

    drawString(x, y + (current_line * line_spacing), subStr, alignment, color);

    // update textRemaining to no longer include what was printed
    // +1 for exclusive bounds, +1 to get passed space/dash
    textRemaining = textRemaining.substring(endIndex + 2 - keepLastChar);

    ++current_line;
  } // end outer while

  return;
} // end drawMultiLnString

/* Initialize e-paper display
 */
void initDisplay() {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, HIGH);
#ifdef DRIVER_WAVESHARE
  display.init(115200, true, 2, false);
#endif
#ifdef DRIVER_DESPI_C02
  display.init(115200, true, 10, false);
#endif
  // remap spi
  SPI.end();
  SPI.begin(PIN_EPD_SCK, PIN_EPD_MISO, PIN_EPD_MOSI, PIN_EPD_CS);

  display.setRotation(0);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setTextWrap(false);
  // display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
  display.firstPage(); // use paged drawing mode, sets fillScreen(GxEPD_WHITE)
  return;
} // end initDisplay

/* Power-off e-paper display
 */
void powerOffDisplay() {
  display.hibernate(); // turns powerOff() and sets controller to deep sleep for
                       // minimum power use
  digitalWrite(PIN_EPD_PWR, LOW);
  return;
} // end initDisplay

void drawCurrentConditions(const dwd_current_t &current,
                           const dwd_daily_t &today, float inTemp,
                           float inHumidity) {
  String dataStr, unitStr;

  // ########## Weather Icon ##########
  // (0,0) (196,196)
  // debug
  // display.drawRect(0, 0, 196, 196, 0);

  display.drawInvertedBitmap(0, 0,
                             getCurrentConditionsBitmap196(current, today), 196,
                             196, GxEPD_BLACK);

  // ########## current temp ##########
  // debug
  // display.drawRect(196, 0, 164, 140, 0);

  dataStr = String(static_cast<int>(std::round(current.condition.temperatur)));
  unitStr = TXT_UNITS_TEMP_CELSIUS;
  const int unit_offset = 20;

  // temperatur
  display.setFont(&FONT_48pt8b_temperature);
  drawString(196 + (164 / 2) - unit_offset, (140 / 2) + (48 / 2) + 15, dataStr,
             CENTER);

  // unit
  display.setFont(&FONT_14pt8b);
  drawString(display.getCursorX(), (196 / 2) - (140 / 2) + (48 / 2) - 10 + 15,
             unitStr, LEFT);

  // ########## INDOR DATA ##########
  // debug
  // display.drawRect(196, 140, 82, 56, 0);

  const int temperatur_offset = -4;

  display.drawInvertedBitmap(196 + temperatur_offset, 140 + ((56 - 48) / 2),
                             house_thermometer_48x48, 48, 48, GxEPD_BLACK);

  // debug
  // display.drawRect(196 + 82, 140, 82, 56, 0);

  display.drawInvertedBitmap(196 + 82, 140 + ((56 - 48) / 2),
                             house_humidity_48x48, 48, 48, GxEPD_BLACK);

  // temperatur
  display.setFont(&FONT_12pt8b);
  if (!std::isnan(inTemp)) {
    dataStr = String(static_cast<int>(std::round(inTemp)));
  } else {
    dataStr = "--";
  }

  dataStr += "\260"; // dagree
  drawString(196 + 48 + temperatur_offset, 140 + (56 / 2) + (12 / 2), dataStr,
             LEFT);

  // humidity
  display.setFont(&FONT_12pt8b);
  if (!std::isnan(inHumidity)) {
    dataStr = String(static_cast<int>(std::round(inHumidity)));
  } else {
    dataStr = "--";
  }

  drawString(196 + 82 + 48, 140 + (56 / 2) + (12 / 2), dataStr, LEFT);
  display.setFont(&FONT_8pt8b);
  drawString(display.getCursorX(), 140 + (56 / 2) + 5, "%", LEFT);
  return;
}

/* This function is responsible for drawing the five day forecast.
 */
void drawForecast(const dwd_daily_t *daily, tm timeInfo) {
  // 5 day, forecast
  String hiStr, loStr;
  String dataStr, unitStr;
  for (int i = 0; i < 5; ++i) {
    int x = 398 + (i * 82);
    // icons
    display.drawInvertedBitmap(x, 98 + 69 / 2 - 32 - 6,
                               getDailyForecastBitmap64(daily[i]), 64, 64,
                               GxEPD_BLACK);
    // day of week label
    display.setFont(&FONT_11pt8b);
    char dayBuffer[8] = {};
    _strftime(dayBuffer, sizeof(dayBuffer), "%a", &timeInfo); // abbrv'd day
    drawString(x + 31 - 2, 98 + 69 / 2 - 32 - 26 - 6 + 16, dayBuffer, CENTER);
    timeInfo.tm_wday = (timeInfo.tm_wday + 1) % 7; // increment to next day

    // high | low
    display.setFont(&FONT_8pt8b);
    drawString(x + 31, 98 + 69 / 2 + 38 - 6 + 12, "|", CENTER);
    hiStr = String(static_cast<int>(std::round(daily[i].temp_max))) + "\260";
    loStr = String(static_cast<int>(std::round(daily[i].temp_min))) + "\260";
    drawString(x + 31 - 4, 98 + 69 / 2 + 38 - 6 + 12, hiStr, RIGHT);
    drawString(x + 31 + 5, 98 + 69 / 2 + 38 - 6 + 12, loStr, LEFT);

// daily forecast precipitation
#if DISPLAY_DAILY_PRECIP
    float dailyPrecip;
#if defined(UNITS_DAILY_PRECIP_POP)
    dailyPrecip = daily[i].pop * 100;
    dataStr = String(static_cast<int>(dailyPrecip));
    unitStr = "%";
#else
    dailyPrecip = daily[i].snow + daily[i].rain;
#if defined(UNITS_DAILY_PRECIP_MILLIMETERS)
    // Round up to nearest mm
    dailyPrecip = std::round(dailyPrecip);
    dataStr = String(static_cast<int>(dailyPrecip));
    unitStr = String(" ") + TXT_UNITS_PRECIP_MILLIMETERS;
#endif
    if (dailyPrecip > 0.0f) {
      display.setFont(&FONT_6pt8b);
      drawString(x + 31, 98 + 69 / 2 + 38 - 6 + 26, dataStr + unitStr, CENTER);
    }
#endif
#endif // DISPLAY_DAILY_PRECIP
  }

  return;
} // end drawForecast

/* This function is responsible for drawing the city string and date
 * information in the top right corner.
 */
void drawLocationDate(const String &city, const String &date) {
  // location, date
  display.setFont(&FONT_16pt8b);
  drawString(DISP_WIDTH - 2, 23, city, RIGHT, ACCENT_COLOR);
  display.setFont(&FONT_12pt8b);
  drawString(DISP_WIDTH - 2, 30 + 4 + 17, date, RIGHT);
  return;
} // end drawLocationDate

/* The % operator in C++ is not a true modulo operator but it instead a
 * remainder operator. The remainder operator and modulo operator are equivalent
 * for positive numbers, but not for negatives. The follow implementation of the
 * modulo operator works for +/-a and +b.
 */
inline int modulo(int a, int b) {
  const int result = a % b;
  return result >= 0 ? result : result + b;
}

/* Convert temperature in celsius to the display y coordinate to be plotted.
 */
int temperatur_to_plot_y(float temperatur, int tempBoundMin, float yPxPerUnit,
                         int yBoundMin) {
  return static_cast<int>(
      std::round(yBoundMin - (yPxPerUnit * (temperatur - tempBoundMin))));
}

void printTime2(tm &timeInfo) {
  Serial.printf("Time: %i-%i-%iT%i:%i\n", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min);
}

/* This function is responsible for drawing the outlook graph for the specified
 * number of hours(up to 48).
 */
void drawOutlookGraph(const dwd_hourly_t *hourly, const dwd_daily_t *daily,
                      tm timeInfo) {

  // offset to current time
  hourly += timeInfo.tm_hour;
  // auto hourly_off = hourly + std::max(0,timeInfo.tm_hour);
  Serial.printf("\nCurrent HOUR = %d\n\n", timeInfo.tm_hour);

  const int xPos0 = 50;
  int xPos1 = DISP_WIDTH;
  const int yPos0 = 216;
  const int yPos1 = DISP_HEIGHT - 46;

  // Graph format
  int yMajorTicks = 5;
  int yTempMajorTicks = 5;
  int xMaxTicks = 12;

  // calculate y max/min and intervals
  float tempMin = hourly[0].temperatur;
  float tempMax = tempMin;
#ifdef UNITS_HOURLY_PRECIP_POP
  float precipMax = hourly[0].precipitation_probability;
#else
  float precipMax = hourly[0].precipitation;
#endif
  float newTemp = 0;

  for (int i = 0; i < HOURLY_GRAPH_MAX; ++i) {
    newTemp = hourly[i].temperatur;
    tempMin = std::min(tempMin, newTemp);
    tempMax = std::max(tempMax, newTemp);
#ifdef UNITS_HOURLY_PRECIP_POP
    precipMax = std::max<float>(precipMax, hourly[i].precipitation_probability);
#else
    precipMax = std::max<float>(precipMax, hourly[i].precipitation);
#endif

    Serial.printf("Temperatur: %f \t Precipitation: %f \t",hourly[i].temperatur, hourly[i].precipitation);
    Serial.printf("Time: %i-%i-%iT%i:%i\n", hourly[i].time.tm_year + 1900,
                hourly[i].time.tm_mon + 1, hourly[i].time.tm_mday, hourly[i].time.tm_hour,
                hourly[i].time.tm_min);
  }

  Serial.printf("MaxPrecipitation: %f \n", precipMax);

  int tempBoundMin = static_cast<int>(tempMin - 1) -
                     modulo(static_cast<int>(tempMin - 1), yTempMajorTicks);
  int tempBoundMax = static_cast<int>(tempMax + 1) +
                     (yTempMajorTicks -
                      modulo(static_cast<int>(tempMax + 1), yTempMajorTicks));

  // while we have to many major ticks then increase the step
  while ((tempBoundMax - tempBoundMin) / yTempMajorTicks > yMajorTicks) {
    yTempMajorTicks += 5;
    tempBoundMin = static_cast<int>(tempMin - 1) -
                   modulo(static_cast<int>(tempMin - 1), yTempMajorTicks);
    tempBoundMax = static_cast<int>(tempMax + 1) +
                   (yTempMajorTicks -
                    modulo(static_cast<int>(tempMax + 1), yTempMajorTicks));
  }
  // while we have not enough major ticks, add to either bound
  while ((tempBoundMax - tempBoundMin) / yTempMajorTicks < yMajorTicks) {
    // add to whatever bound is closer to the actual min/max
    if (tempMin - tempBoundMin <= tempBoundMax - tempMax) {
      tempBoundMin -= yTempMajorTicks;
    } else {
      tempBoundMax += yTempMajorTicks;
    }
  }

#ifdef UNITS_HOURLY_PRECIP_POP
  xPos1 = DISP_WIDTH - 23;
  float precipBoundMax;
  if (precipMax > 0) {
    precipBoundMax = 100.0f;
  } else {
    precipBoundMax = 0.0f;
  }
#else
#ifdef UNITS_HOURLY_PRECIP_MILLIMETERS
  xPos1 = DISP_WIDTH - 24;
  float precipBoundMax = std::ceil(precipMax); // Round up to nearest mm
  int yPrecipMajorTickDecimals = (precipBoundMax < 10);
#endif
  float yPrecipMajorTickValue = precipBoundMax / yMajorTicks;
  float precipRoundingMultiplier = std::pow(10.f, yPrecipMajorTickDecimals);
#endif

  if (precipBoundMax > 0) { // fill need extra room for labels
    xPos1 -= 23;
  }

  // ensure that the scaling is not missleading
  if ( precipBoundMax > 0 && precipBoundMax < 3.0f) {
    precipBoundMax = 3.0f;
  }

  // draw x axis
  display.drawLine(xPos0, yPos1, xPos1, yPos1, GxEPD_BLACK);
  display.drawLine(xPos0, yPos1 - 1, xPos1, yPos1 - 1, GxEPD_BLACK);

  // draw y axis
  float yInterval = (yPos1 - yPos0) / static_cast<float>(yMajorTicks);
  for (int i = 0; i <= yMajorTicks; ++i) {
    String dataStr;
    int yTick = static_cast<int>(yPos0 + (i * yInterval));
    display.setFont(&FONT_8pt8b);
    // Temperature
    dataStr = String(tempBoundMax - (i * yTempMajorTicks));
    dataStr += "\260";

    drawString(xPos0 - 8, yTick + 4, dataStr, RIGHT, ACCENT_COLOR);

    if (precipBoundMax > 0) { // don't labels if precip is 0
#ifdef UNITS_HOURLY_PRECIP_POP
                              // PoP
      dataStr = String(100 - (i * 20));
      String precipUnit = "%";
#else
                              // Precipitation volume
      float precipTick = precipBoundMax - (i * yPrecipMajorTickValue);
      precipTick = std::round(precipTick * precipRoundingMultiplier) /
                   precipRoundingMultiplier;
      dataStr = String(precipTick, yPrecipMajorTickDecimals);
#ifdef UNITS_HOURLY_PRECIP_MILLIMETERS
      String precipUnit = String(" ") + TXT_UNITS_PRECIP_MILLIMETERS;
#endif
#endif

      drawString(xPos1 + 8, yTick + 4, dataStr, LEFT);
      display.setFont(&FONT_5pt8b);
      drawString(display.getCursorX(), yTick + 4, precipUnit, LEFT);
    } // end draw labels if precip is >0

    // draw dotted line
    if (i < yMajorTicks) {
      for (int x = xPos0; x <= xPos1 + 1; x += 3) {
        display.drawPixel(x, yTick + (yTick % 2), GxEPD_BLACK);
      }
    }
  }

  int hourInterval =
      static_cast<int>(ceil(HOURLY_GRAPH_MAX / static_cast<float>(xMaxTicks)));
  float xInterval = (xPos1 - xPos0 - 1) / static_cast<float>(HOURLY_GRAPH_MAX);
  display.setFont(&FONT_8pt8b);

  // precalculate all x and y coordinates for temperature values
  float yPxPerUnit =
      (yPos1 - yPos0) / static_cast<float>(tempBoundMax - tempBoundMin);
  std::vector<int> x_t;
  std::vector<int> y_t;
  x_t.reserve(HOURLY_GRAPH_MAX);
  y_t.reserve(HOURLY_GRAPH_MAX);
  for (int i = 0; i < HOURLY_GRAPH_MAX; ++i) {
    y_t[i] = temperatur_to_plot_y(hourly[i].temperatur, tempBoundMin, yPxPerUnit, yPos1);
    x_t[i] = static_cast<int>(
        std::round(xPos0 + (i * xInterval) + (0.5 * xInterval)));
  }

#if DISPLAY_HOURLY_ICONS
  int day_idx = 0;
#endif
  display.setFont(&FONT_8pt8b);
  for (int i = 0; i < HOURLY_GRAPH_MAX; ++i) {
    int xTick = static_cast<int>(xPos0 + (i * xInterval));
    int x0_t, x1_t, y0_t, y1_t;

    if (i > 0) {
      // temperature
      x0_t = x_t[i - 1];
      x1_t = x_t[i];
      y0_t = y_t[i - 1];
      y1_t = y_t[i];
      // graph temperature
      display.drawLine(x0_t, y0_t, x1_t, y1_t, ACCENT_COLOR);
      display.drawLine(x0_t, y0_t + 1, x1_t, y1_t + 1, ACCENT_COLOR);
      display.drawLine(x0_t - 1, y0_t, x1_t - 1, y1_t, ACCENT_COLOR);

      // draw hourly bitmap
#if DISPLAY_HOURLY_ICONS
      if (daily[day_idx].time.tm_mday != hourly[i].time.tm_mday) {
        ++day_idx;
      }

      if ((i % hourInterval) == 0) // skip first and last tick
      {
        int y_b = INT_MAX;
        // find the highest (lowest in coordinate value) temperature point that
        // exists within the width of the icon.
        // find closest point above the temperature line where the icon won't
        // interect the temperature line.
        // y = mx + b
        int span = static_cast<int>(std::round(16 / xInterval));
        int l_idx = std::max(i - 1 - span, 0);
        int r_idx = std::min(i + span, HOURLY_GRAPH_MAX - 1);
        // left intersecting slope
        float m_l = (y_t[l_idx + 1] - y_t[l_idx]) / xInterval;
        int x_l = xTick - 16 - x_t[l_idx];
        int y_l = static_cast<int>(std::round(m_l * x_l + y_t[l_idx]));
        y_b = std::min(y_l, y_b);
        // right intersecting slope
        float m_r = (y_t[r_idx] - y_t[r_idx - 1]) / xInterval;
        int x_r = xTick + 16 - x_t[r_idx - 1];
        int y_r = static_cast<int>(std::round(m_r * x_r + y_t[r_idx - 1]));
        y_b = std::min(y_r, y_b);
        // any peaks in between
        for (int idx = l_idx + 1; idx < r_idx; ++idx) {
          y_b = std::min(y_t[idx], y_b);
        }
        const uint8_t *bitmap =
            getHourlyForecastBitmap32(hourly[i], daily[day_idx]);
        display.drawInvertedBitmap(xTick - 16, y_b - 32, bitmap, 32, 32,
                                   GxEPD_BLACK);
      }
#endif
    }

#ifdef UNITS_HOURLY_PRECIP_POP
    float precipVal = hourly[i].precipitation_probability;
#else
    float precipVal = hourly[i].precipitation;
#ifdef UNITS_HOURLY_PRECIP_CENTIMETERS
    precipVal = millimeters_to_centimeters(precipVal);
#endif
#endif

    x0_t = static_cast<int>(std::round(xPos0 + 1 + (i * xInterval)));
    x1_t = static_cast<int>(std::round(xPos0 + 1 + ((i + 1) * xInterval)));
    yPxPerUnit = (yPos1 - yPos0) / precipBoundMax;
    y0_t = static_cast<int>(std::round(yPos1 - (yPxPerUnit * (precipVal))));
    y1_t = yPos1;

    // graph Precipitation
    for (int y = y1_t - 1; y > y0_t; y -= 2) {
      for (int x = x0_t + (x0_t % 2); x < x1_t; x += 2) {
        display.drawPixel(x, y, GxEPD_BLACK);
      }
    }

    if ((i % hourInterval) == 0) {
      // draw x tick marks
      display.drawLine(xTick, yPos1 + 1, xTick, yPos1 + 4, GxEPD_BLACK);
      display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);
      // draw x axis labels
      char timeBuffer[12] = {}; // big enough to accommodate "hh:mm:ss am"
      tm timeInfo = hourly[i].time;
      _strftime(timeBuffer, sizeof(timeBuffer), HOUR_FORMAT, &timeInfo);
      drawString(xTick, yPos1 + 1 + 12 + 4 + 3, timeBuffer, CENTER);
    }
  }

  // draw the last tick mark
  if ((HOURLY_GRAPH_MAX % hourInterval) == 0) {
    int xTick =
        static_cast<int>(std::round(xPos0 + (HOURLY_GRAPH_MAX * xInterval)));
    // draw x tick marks
    display.drawLine(xTick, yPos1 + 1, xTick, yPos1 + 4, GxEPD_BLACK);
    display.drawLine(xTick + 1, yPos1 + 1, xTick + 1, yPos1 + 4, GxEPD_BLACK);
    // draw x axis labels
    char timeBuffer[12] = {}; // big enough to accommodate "hh:mm:ss am"
    tm timeInfo = hourly[HOURLY_GRAPH_MAX - 1].time;
    timeInfo.tm_hour += 1;
    _strftime(timeBuffer, sizeof(timeBuffer), HOUR_FORMAT, &timeInfo);
    drawString(xTick, yPos1 + 1 + 12 + 4 + 3, timeBuffer, CENTER);
  }

  return;
} // end drawOutlookGraph

/* This function is responsible for drawing the status bar along the bottom of
 * the display.
 */
void drawStatusBar(const String &statusStr, const String &refreshTimeStr,
                   int rssi, uint32_t batVoltage) {
  String dataStr;
  uint16_t dataColor = GxEPD_BLACK;
  display.setFont(&FONT_6pt8b);
  int pos = DISP_WIDTH - 2;
  const int sp = 2;

#if BATTERY_MONITORING
  // battery - (expecting 3.7v LiPo)
  uint32_t batPercent =
      calcBatPercent(batVoltage, MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
#if defined(DISP_3C_B) || defined(DISP_7C_F)
  if (batVoltage < WARN_BATTERY_VOLTAGE) {
    dataColor = ACCENT_COLOR;
  }
#endif
  dataStr = String(batPercent) + "%";
#if STATUS_BAR_EXTRAS_BAT_VOLTAGE
  dataStr += " (" + String(std::round(batVoltage / 10.f) / 100.f, 2) + "v)";
#endif
  drawString(pos, DISP_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
  pos -= getStringWidth(dataStr) + 25;
  display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 17,
                             getBatBitmap24(batPercent), 24, 24, dataColor);
  pos -= sp + 9;
#endif

  // WiFi
  dataStr = String(getWiFidesc(rssi));
  dataColor = rssi >= -70 ? GxEPD_BLACK : ACCENT_COLOR;
#if STATUS_BAR_EXTRAS_WIFI_RSSI
  if (rssi != 0) {
    dataStr += " (" + String(rssi) + "dBm)";
  }
#endif
  drawString(pos, DISP_HEIGHT - 1 - 2, dataStr, RIGHT, dataColor);
  pos -= getStringWidth(dataStr) + 19;
  display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 13, getWiFiBitmap16(rssi),
                             16, 16, dataColor);
  pos -= sp + 8;

  // last refresh
  dataColor = GxEPD_BLACK;
  drawString(pos, DISP_HEIGHT - 1 - 2, refreshTimeStr, RIGHT, dataColor);
  pos -= getStringWidth(refreshTimeStr) + 25;
  display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 21, wi_refresh_32x32, 32,
                             32, dataColor);
  pos -= sp;

  // status
  dataColor = ACCENT_COLOR;
  if (!statusStr.isEmpty()) {
    drawString(pos, DISP_HEIGHT - 1 - 2, statusStr, RIGHT, dataColor);
    pos -= getStringWidth(statusStr) + 24;
    display.drawInvertedBitmap(pos, DISP_HEIGHT - 1 - 18, error_icon_24x24, 24,
                               24, dataColor);
  }

  return;
} // end drawStatusBar

/* This function is responsible for drawing prominent error messages to the
 * screen.
 *
 * If error message line 2 (errMsgLn2) is empty, line 1 will be automatically
 * wrapped.
 */
void drawError(const uint8_t *bitmap_196x196, const String &errMsgLn1,
               const String &errMsgLn2) {
  display.setFont(&FONT_26pt8b);
  if (!errMsgLn2.isEmpty()) {
    drawString(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21, errMsgLn1,
               CENTER);
    drawString(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21 + 55, errMsgLn2,
               CENTER);
  } else {
    drawMultiLnString(DISP_WIDTH / 2, DISP_HEIGHT / 2 + 196 / 2 + 21, errMsgLn1,
                      CENTER, DISP_WIDTH - 200, 2, 55);
  }
  display.drawInvertedBitmap(DISP_WIDTH / 2 - 196 / 2,
                             DISP_HEIGHT / 2 - 196 / 2 - 21, bitmap_196x196,
                             196, 196, ACCENT_COLOR);
  return;
} // end drawError
