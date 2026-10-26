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
 
#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>

// Selection of the logged severities and the buffer sizes come from user_config.h.
// It must be visible in every file that includes this header, which is why it is
// included here. (user_config.h may use the LOG_LEVEL_* names below.)
#include "../../sysconfig.h"

// ---- Severity groups (compile time) ----
#define LOG_LEVEL_ALL         1   // emerg ... debug
#define LOG_LEVEL_NO_DEBUG    2   // emerg ... info
#define LOG_LEVEL_PRODUCTION  3   // emerg ... warn

// Older user_config.h files only have "#define DEBUG": treat it as "everything".
#if !defined(LOG_LEVEL) && defined(DEBUG)
#define LOG_LEVEL LOG_LEVEL_ALL
#endif

// Severity values as numbers, for the preprocessor (RFC 5424, section 6.2.1).
#define LOG_SEVERITY_EMERG   0
#define LOG_SEVERITY_ALERT   1
#define LOG_SEVERITY_CRIT    2
#define LOG_SEVERITY_ERROR   3
#define LOG_SEVERITY_WARN    4
#define LOG_SEVERITY_NOTICE  5
#define LOG_SEVERITY_INFO    6
#define LOG_SEVERITY_DEBUG   7

// LOG_ENABLED:      1 if logging is compiled in at all
// LOG_MAX_SEVERITY: highest severity number that is compiled in (-1: none)
#if defined(LOG_LEVEL)
  #define LOG_ENABLED 1
  #if LOG_LEVEL == LOG_LEVEL_ALL
    #define LOG_MAX_SEVERITY LOG_SEVERITY_DEBUG
  #elif LOG_LEVEL == LOG_LEVEL_NO_DEBUG
    #define LOG_MAX_SEVERITY LOG_SEVERITY_INFO
  #elif LOG_LEVEL == LOG_LEVEL_PRODUCTION
    #define LOG_MAX_SEVERITY LOG_SEVERITY_WARN
  #else
    #error "LOG_LEVEL must be LOG_LEVEL_ALL, LOG_LEVEL_NO_DEBUG or LOG_LEVEL_PRODUCTION"
  #endif
#else
  #define LOG_ENABLED 0
  #define LOG_MAX_SEVERITY (-1)
#endif

// ---- Defaults for the optional settings ----
#ifndef LOG_TO_SERIAL
#define LOG_TO_SERIAL 1
#endif

#ifndef LOG_BUFFER_ENTRIES
#define LOG_BUFFER_ENTRIES 64
#endif

#ifndef LOG_MESSAGE_LENGTH
#define LOG_MESSAGE_LENGTH 112
#endif

#if LOG_BUFFER_ENTRIES < 1 || LOG_MESSAGE_LENGTH < 16
#error "LOG_BUFFER_ENTRIES must be >= 1 and LOG_MESSAGE_LENGTH >= 16"
#endif

// Logger: messages go to the serial line (Serial Monitor) and into a ring buffer
// in RAM, from which the web interface shows them.
//
// Serial line format, loosely following RFC 5424:
//   <severity> TIMESTAMP message
//   e.g.  <warn> 2026-10-18T10:31:07.482Z WiFi: connection lost
//
// Rules:
//  - There is exactly one instance: the global "logger".
//  - log() may be called from any task, but NOT from an interrupt handler.
//  - log() never waits for other tasks, except for a copy of one entry that takes
//    well under a microsecond (spinlock).
//  - Nothing is ever written to flash.
//  - Messages that were logged before begin() are buffered too (not printed).
class Logger {
public:
    // Severity values as defined in RFC 5424, section 6.2.1.
    // (CamelCase on purpose: avoids clashes with macros.)
    enum class Severity : uint8_t {
        Emerg  = LOG_SEVERITY_EMERG,  // system is unusable
        Alert  = LOG_SEVERITY_ALERT,  // action must be taken immediately
        Crit   = LOG_SEVERITY_CRIT,   // critical conditions
        Error  = LOG_SEVERITY_ERROR,  // error conditions
        Warn   = LOG_SEVERITY_WARN,   // warning conditions
        Notice = LOG_SEVERITY_NOTICE, // normal but significant condition
        Info   = LOG_SEVERITY_INFO,   // informational messages
        Debug  = LOG_SEVERITY_DEBUG   // debug-level messages
    };

    // One buffered log entry.
    struct Entry {
        uint32_t id;            // running number, starts at 1 after each boot
        uint32_t epochSeconds;  // UTC seconds since 1970; 0 = clock not set yet
        uint32_t uptimeMs;      // millis() when the message was logged
        uint16_t millisPart;    // milliseconds of the time stamp (0..999)
        Severity severity;
        char     message[LOG_MESSAGE_LENGTH];   // truncated if too long
    };

    static constexpr size_t BUFFER_ENTRIES = LOG_BUFFER_ENTRIES;

    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Call once in setup(). On USB serial the baud rate is ignored, but set the
    // monitor to the same value anyway. Does nothing with Serial if LOG_TO_SERIAL is 0.
    void begin(unsigned long baud = 115200);

    // printf-style message with severity and RFC 5424 timestamp (UTC).
    // While the clock is not set yet, the timestamp is the RFC 5424 NILVALUE "-".
    void log(Severity severity, const char* format, ...) __attribute__((format(printf, 3, 4)));

    // ---- Reading the buffer (used by the web interface) ----

    // Random number chosen at begin(). A different value means "the device restarted".
    uint32_t bootId() const;

    // Id of the newest entry; 0 = buffer is empty. The buffer holds the entries
    // newestId() - BUFFER_ENTRIES + 1 ... newestId().
    uint32_t newestId() const;

    // Copies the entry with this id. Returns false if it is not (or no longer) in the buffer.
    bool copyEntry(uint32_t id, Entry& out) const;

    // "emerg", "alert", "crit", "error", "warn", "notice", "info", "debug".
    static const char* severityName(Severity severity);

    // RFC 5424 TIMESTAMP of an entry: "YYYY-MM-DDTHH:MM:SS.mmmZ" (UTC), or "-" if unknown.
    // 'size' should be at least 32.
    static void formatTimestamp(const Entry& entry, char* out, size_t size);
};

extern Logger logger;

// ---- Logging macros ----
// A macro of a severity that is not part of the selected group expands to nothing:
// its arguments, including the text, are not compiled into the binary.

#if LOG_ENABLED
#define LOGGER_BEGIN(...)  logger.begin(__VA_ARGS__)
#else
#define LOGGER_BEGIN(...)  ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_EMERG
#define LOGGER_EMERG(...)  logger.log(Logger::Severity::Emerg,  __VA_ARGS__)
#else
#define LOGGER_EMERG(...)  ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_ALERT
#define LOGGER_ALERT(...)  logger.log(Logger::Severity::Alert,  __VA_ARGS__)
#else
#define LOGGER_ALERT(...)  ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_CRIT
#define LOGGER_CRIT(...)   logger.log(Logger::Severity::Crit,   __VA_ARGS__)
#else
#define LOGGER_CRIT(...)   ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_ERROR
#define LOGGER_ERROR(...)  logger.log(Logger::Severity::Error,  __VA_ARGS__)
#else
#define LOGGER_ERROR(...)  ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_WARN
#define LOGGER_WARN(...)   logger.log(Logger::Severity::Warn,   __VA_ARGS__)
#else
#define LOGGER_WARN(...)   ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_NOTICE
#define LOGGER_NOTICE(...) logger.log(Logger::Severity::Notice, __VA_ARGS__)
#else
#define LOGGER_NOTICE(...) ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_INFO
#define LOGGER_INFO(...)   logger.log(Logger::Severity::Info,   __VA_ARGS__)
#else
#define LOGGER_INFO(...)   ((void)0)
#endif

#if LOG_MAX_SEVERITY >= LOG_SEVERITY_DEBUG
#define LOGGER_DEBUG(...)  logger.log(Logger::Severity::Debug,  __VA_ARGS__)
#else
#define LOGGER_DEBUG(...)  ((void)0)
#endif

// Shortcut for existing code: logs with severity "info".
#define LOGGER_LOG(...)    LOGGER_INFO(__VA_ARGS__)

#endif  // LOGGER_H