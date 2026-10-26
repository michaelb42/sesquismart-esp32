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
 
#include "log_json.h"
#include <stdio.h>
#include "../utils/logger.h"

namespace {

// Writes text as a JSON string including the quotes. Handles quotes, backslashes and
// control characters, so that a message can never break the JSON.
void writeJsonString(Print& out, const char* text) {
    char buf[LOG_MESSAGE_LENGTH * 6 + 3];     // worst case: every character becomes \u00XX
    size_t n = 0;

    buf[n++] = '"';
    for (const char* p = text; *p != '\0'; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        switch (c) {
            case '"':  buf[n++] = '\\'; buf[n++] = '"';  break;
            case '\\': buf[n++] = '\\'; buf[n++] = '\\'; break;
            case '\n': buf[n++] = '\\'; buf[n++] = 'n';  break;
            case '\r': buf[n++] = '\\'; buf[n++] = 'r';  break;
            case '\t': buf[n++] = '\\'; buf[n++] = 't';  break;
            default:
                if (c < 0x20) {
                    n += static_cast<size_t>(snprintf(buf + n, 7, "\\u%04x", c));
                } else {
                    buf[n++] = static_cast<char>(c);
                }
        }
    }
    buf[n++] = '"';
    buf[n] = '\0';

    out.print(buf);
}

}  // namespace

void writeLogsJson(Print& out, uint32_t clientBootId, uint32_t since, size_t maxEntries) {
    const uint32_t boot = logger.bootId();

    // Different boot id: the device restarted, the client's "since" is meaningless.
    const bool reset = (since != 0) && (clientBootId != boot);
    if (reset) {
        since = 0;
    }

    // Send the newest entries after "since", at most maxEntries of them.
    const uint32_t newest = logger.newestId();
    const uint32_t count  = (newest > since) ? (newest - since) : 0;
    const uint32_t first  = (count > maxEntries) ? (newest - static_cast<uint32_t>(maxEntries) + 1)
                                                 : (since + 1);

    bool gap = (since != 0) && (first > since + 1);

    out.printf("{\"boot\":%lu,\"reset\":%s,\"entries\":[",
               static_cast<unsigned long>(boot), reset ? "true" : "false");

    bool firstEntry = true;
    for (uint32_t id = first; count > 0 && id <= newest; id++) {
        Logger::Entry entry;
        if (!logger.copyEntry(id, entry)) {
            if (since != 0) {
                gap = true;              // overwritten in the meantime
            }
            continue;
        }

        if (!firstEntry) {
            out.print(',');
        }
        firstEntry = false;

        out.printf("{\"id\":%lu,\"time\":", static_cast<unsigned long>(entry.id));
        if (entry.epochSeconds == 0) {
            out.print("null");
        } else {
            char timestamp[32];
            Logger::formatTimestamp(entry, timestamp, sizeof timestamp);
            out.printf("\"%s\"", timestamp);
        }
        out.printf(",\"uptime_ms\":%lu,\"severity\":\"%s\",\"message\":",
                   static_cast<unsigned long>(entry.uptimeMs),
                   Logger::severityName(entry.severity));
        writeJsonString(out, entry.message);
        out.print('}');
    }

    out.printf("],\"gap\":%s,\"newest_id\":%lu}",
               gap ? "true" : "false", static_cast<unsigned long>(newest));
}