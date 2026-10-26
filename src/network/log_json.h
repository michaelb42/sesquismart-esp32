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
 
#ifndef LOG_JSON_H
#define LOG_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <Print.h>

// Writes buffered log entries as JSON to 'out' (no String, no ArduinoJson).
//
//   clientBootId, since: what the client received last time (0, 0 for the first request).
//   maxEntries:          upper limit; if there are more new entries, the newest ones are sent.
//
// Answer:
//   {"boot":123,"reset":false,
//    "entries":[{"id":7,"time":"2026-10-20T12:00:01.123Z","uptime_ms":4711,
//                "severity":"warn","message":"..."}, ...],
//    "gap":false,"newest_id":7}
//
//   reset:  the device restarted since the client's last request; the client must
//           discard what it has.
//   gap:    entries between 'since' and the first sent entry are missing (overwritten
//           in the buffer or cut by maxEntries).
//   time:   null if the clock was not set when the message was logged.
void writeLogsJson(Print& out, uint32_t clientBootId, uint32_t since, size_t maxEntries);

#endif  // LOG_JSON_H