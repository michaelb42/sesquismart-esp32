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
 
#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>

// Synchronizes the system clock with two NTP servers and provides the local
// date and time (Germany: CET/CEST with automatic daylight saving) as strings.
class TimeSync {
public:
    TimeSync() = default;

    // Call once in setup(). Does not block: the clock is set in the background
    // as soon as WiFi is connected and a server answers.
    void begin();

    // True once the clock has been set by an NTP server.
    bool isSynced() const;

    String getDate() const;          // "TT.MM.JJJJ"            e.g. "18.10.2026"
    String getTime() const;          // "HH:MM:SS"              e.g. "12:23:45"
    String getDateTime() const;      // "TT.MM.JJJJ HH:MM:SS"   e.g. "18.10.2026 12:23:45"

    // ISO 8601 extended format, always UTC (Zulu time), millisecond resolution:
    // "YYYY-MM-DDTHH:MM:SS.mmmZ"   e.g. "2026-10-18T10:23:45.123Z"
    String getIsoDateTime() const;
};

#endif // TIME_SYNC_H