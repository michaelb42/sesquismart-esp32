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
 
#include "time_sync.h"
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "../../sysconfig.h"          // NTP_SERVER_1, NTP_SERVER_2
#include "../utils/logger.h"

namespace {
const char* const TS_NTP_SERVER_1 = NTP_SERVER_1;
const char* const TS_NTP_SERVER_2 = NTP_SERVER_2;

// POSIX time zone for Germany: CET (UTC+1), CEST (UTC+2) from the last Sunday
// in March 02:00 to the last Sunday in October 03:00.
const char* const TS_TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

const int TS_MIN_VALID_YEAR = 2020;     // an earlier year means the clock is not set yet

const char* const TS_PLACEHOLDER_DATE = "--.--.----";
const char* const TS_PLACEHOLDER_TIME = "--:--:--";
const char* const TS_PLACEHOLDER_ISO  = "----------T--:--:--.---Z";

// Fills 'out' with the local time. Returns false while the clock is not set.
bool getLocalTm(struct tm& out) {
    const time_t now = time(nullptr);
    localtime_r(&now, &out);
    return (out.tm_year + 1900) >= TS_MIN_VALID_YEAR;
}

#ifdef LOG_ENABLED
void onTimeSynced(struct timeval*) {
    struct tm t;
    if (getLocalTm(t)) {
        LOGGER_INFO("NTP: time synchronized: %02d.%02d.%04d %02d:%02d:%02d local time",
                  t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
                  t.tm_hour, t.tm_min, t.tm_sec);
    }
}
#endif
}  // namespace

void TimeSync::begin() {
#ifdef LOG_ENABLED
    sntp_set_time_sync_notification_cb(onTimeSynced);
#endif

    // The clock itself runs in UTC; the German local time comes from the TZ string.
    configTime(0, 0, TS_NTP_SERVER_1, TS_NTP_SERVER_2);
    setenv("TZ", TS_TIME_ZONE, 1);      // after configTime(), which sets its own TZ
    tzset();

    LOGGER_INFO("NTP: servers %s and %s, time zone %s",
              TS_NTP_SERVER_1, TS_NTP_SERVER_2, TS_TIME_ZONE);
}

bool TimeSync::isSynced() const {
    struct tm t;
    return getLocalTm(t);
}

String TimeSync::getDate() const {
    struct tm t;
    if (!getLocalTm(t)) {
        return String(TS_PLACEHOLDER_DATE);
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    return String(buffer);
}

String TimeSync::getTime() const {
    struct tm t;
    if (!getLocalTm(t)) {
        return String(TS_PLACEHOLDER_TIME);
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return String(buffer);
}

String TimeSync::getDateTime() const {
    struct tm t;
    if (!getLocalTm(t)) {
        return String(TS_PLACEHOLDER_DATE) + " " + TS_PLACEHOLDER_TIME;
    }
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d %02d:%02d:%02d",
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
    return String(buffer);
}

String TimeSync::getIsoDateTime() const {
    struct timeval tv;
    gettimeofday(&tv, nullptr);             // seconds and microseconds from one clock read

    const time_t seconds = tv.tv_sec;
    struct tm t;
    gmtime_r(&seconds, &t);                 // UTC, not local time
    if ((t.tm_year + 1900) < TS_MIN_VALID_YEAR) {
        return String(TS_PLACEHOLDER_ISO);
    }

    const int millisecond = static_cast<int>(tv.tv_usec / 1000);

    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec, millisecond);
    return String(buffer);
}
