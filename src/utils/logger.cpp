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
 
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#if __has_include(<esp_random.h>)
#include <esp_random.h>
#else
#include <esp_system.h>
#endif

namespace {
const uint8_t  LGR_SEVERITY_COUNT = 8;
const char* const LGR_SEVERITY_NAMES[LGR_SEVERITY_COUNT] = {
    "emerg", "alert", "crit", "error", "warn", "notice", "info", "debug"
};

const size_t   LGR_TIMESTAMP_SIZE    = 32;
const char* const LGR_TIMESTAMP_NIL  = "-";             // RFC 5424 NILVALUE: timestamp unknown
const int64_t  LGR_MIN_VALID_EPOCH   = 1577836800LL;    // 2020-01-01: earlier means "clock not set"
const uint32_t LGR_BEGIN_TIMEOUT_MS  = 2000;            // max. wait for the USB serial connection
}  // namespace

Logger logger;

const char* Logger::severityName(Severity severity) {
    uint8_t index = static_cast<uint8_t>(severity);
    if (index >= LGR_SEVERITY_COUNT) {
        index = LGR_SEVERITY_COUNT - 1;           // out-of-range value: treat as "debug"
    }
    return LGR_SEVERITY_NAMES[index];
}

void Logger::formatTimestamp(const Entry& entry, char* out, size_t size) {
    if (entry.epochSeconds == 0) {
        snprintf(out, size, "%s", LGR_TIMESTAMP_NIL);
        return;
    }

    const time_t seconds = static_cast<time_t>(entry.epochSeconds);
    struct tm t;
    gmtime_r(&seconds, &t);

    snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec,
             static_cast<unsigned>(entry.millisPart));
}

#if LOG_ENABLED

namespace {
// The one and only buffer. Static RAM, no heap.
Logger::Entry lgrSlots[LOG_BUFFER_ENTRIES];
uint32_t      lgrNewestId     = 0;      // id of the newest entry, 0 = empty
uint32_t      lgrBootId       = 0;
bool          lgrSerialReady  = false;
portMUX_TYPE  lgrMux          = portMUX_INITIALIZER_UNLOCKED;
}  // namespace

void Logger::begin(unsigned long baud) {
    lgrBootId = esp_random();

#if LOG_TO_SERIAL
    Serial.begin(baud);

    // Native USB serial: wait until the monitor is connected, but not forever,
    // so the sketch also runs without a connected monitor.
    const uint32_t start = millis();
    while (!Serial && (millis() - start) < LGR_BEGIN_TIMEOUT_MS) {
        delay(10);
    }
    lgrSerialReady = true;
#else
    (void)baud;
#endif
}

void Logger::log(Severity severity, const char* format, ...) {
    Entry entry;
    entry.severity = severity;

    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);   // truncates
    va_end(args);

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    entry.epochSeconds = (tv.tv_sec >= LGR_MIN_VALID_EPOCH) ? static_cast<uint32_t>(tv.tv_sec) : 0;
    entry.millisPart   = static_cast<uint16_t>(tv.tv_usec / 1000);
    entry.uptimeMs     = millis();

    // Only this short copy is done under the lock.
    portENTER_CRITICAL(&lgrMux);
    entry.id = ++lgrNewestId;
    lgrSlots[entry.id % LOG_BUFFER_ENTRIES] = entry;
    portEXIT_CRITICAL(&lgrMux);

#if LOG_TO_SERIAL
    if (lgrSerialReady) {
        char timestamp[LGR_TIMESTAMP_SIZE];
        formatTimestamp(entry, timestamp, sizeof(timestamp));
        Serial.printf("<%s> %s %s\n", severityName(severity), timestamp, entry.message);
    }
#endif
}

uint32_t Logger::bootId() const {
    return lgrBootId;
}

uint32_t Logger::newestId() const {
    portENTER_CRITICAL(&lgrMux);
    const uint32_t id = lgrNewestId;
    portEXIT_CRITICAL(&lgrMux);
    return id;
}

bool Logger::copyEntry(uint32_t id, Entry& out) const {
    bool found = false;
    portENTER_CRITICAL(&lgrMux);
    if (id != 0 && id <= lgrNewestId && (lgrNewestId - id) < LOG_BUFFER_ENTRIES) {
        out = lgrSlots[id % LOG_BUFFER_ENTRIES];
        found = true;
    }
    portEXIT_CRITICAL(&lgrMux);
    return found;
}

#else  // logging not compiled in: no buffer, no code

void Logger::begin(unsigned long) {}
void Logger::log(Severity, const char*, ...) {}
uint32_t Logger::bootId() const { return 0; }
uint32_t Logger::newestId() const { return 0; }
bool Logger::copyEntry(uint32_t, Entry&) const { return false; }

#endif  // LOG_ENABLED