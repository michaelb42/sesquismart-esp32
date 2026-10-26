/* 
 * This file is part of sesquismart-esp32
 * (https://github.com/michaelb42/sesquismart-esp32).
 * 
 * Copyright (C) 2026 Michael Brakemeier (michaelb42).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SYSCONFIG_H
#define SYSCONFIG_H

// *** semi-smart add-on hardware ***
// hmi buttons, hard-wired on the semi-smart mainboard
#define PIN_ACTION_BUTTON 6
#define PIN_BUTTON_PLUS 7
#define PIN_BUTTON_MINUS 8

// *** sesquismart-esp32 piggyback ***
// Neopixel-style/WS2811 RGB-LED strip data
#define PIN_RGB_DATA 23 // pin A6/D23

// *** Networking ***
// Name of this device in the network (DHCP hostname) and in the HTTP "Server" header.
#define NET_HOSTNAME "sesquismart-esp32"
#define NET_SERVERNAME "SesquiSmartESP32"

// NTP servers to use
#define NTP_SERVER_1 "192.168.178.1"    // probably best use your router as first instance
#define NTP_SERVER_2 "ntp1.t-online.de" // german provider, change to your own

// *** Logging ***
// Which severities are compiled in AND kept in the RAM buffer / sent to Serial:
//   LOG_LEVEL_ALL         emerg ... debug
//   LOG_LEVEL_NO_DEBUG    emerg ... info
//   LOG_LEVEL_PRODUCTION  emerg ... warn
// Not defined: no logging at all.
#define LOG_LEVEL LOG_LEVEL_ALL

// Optional (defaults in logger.h):
// #define LOG_TO_SERIAL      0     // 1 = also print to Serial (default)
// #define LOG_BUFFER_ENTRIES 64    // number of entries in the RAM buffer
// #define LOG_MESSAGE_LENGTH 112   // max. characters per entry incl. terminator

// *** Default values (DO NOT CHANGE unless you know ...!) ***
#define DEFAULT_TEMP 40           // 40°C default chamber temperature
#define DEFAULT_HUM 25            // 25% default target humidity
#define DEFAULT_DRY_DURATION 120  // Default drying duration in minutes

#endif // SYSCONFIG_H