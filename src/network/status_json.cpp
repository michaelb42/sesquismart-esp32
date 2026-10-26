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
 
#include "status_json.h"
#include <Arduino.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

namespace {

// Appends text to a fixed buffer and remembers if it did not fit.
class JsonOut {
public:
    JsonOut(char* buf, size_t size) : _buf(buf), _size(size) {
        if (_size > 0) {
            _buf[0] = '\0';
        }
    }

    void add(const char* format, ...) {
        if (_overflow || _len >= _size) {
            _overflow = true;
            return;
        }
        va_list args;
        va_start(args, format);
        const int n = vsnprintf(_buf + _len, _size - _len, format, args);
        va_end(args);
        if (n < 0 || static_cast<size_t>(n) >= _size - _len) {
            _overflow = true;
        } else {
            _len += static_cast<size_t>(n);
        }
    }

    // One decimal, or null if the value is not a finite number.
    void addNumber(float value) {
        if (isfinite(value)) {
            add("%.1f", static_cast<double>(value));
        } else {
            add("null");
        }
    }

    bool ok() const { return !_overflow; }

private:
    char*  _buf;
    size_t _size;
    size_t _len = 0;
    bool   _overflow = false;
};

const char* jsonBool(bool value) {
    return value ? "true" : "false";
}

}  // namespace

// Example:
// {"available":true,"age_ms":120,
//  "chamber":{"valid":true,"temperature":23.4,"humidity":45.2},
//  "heater":{"on":false},"fans":{"speed_percent":0}}
bool buildStatusJson(const DeviceStatus* status, char* buf, size_t size) {
    JsonOut json(buf, size);

    if (status == nullptr) {
        json.add("{\"available\":false}");
        return json.ok();
    }

    json.add("{\"available\":true,\"age_ms\":%lu",
             static_cast<unsigned long>(millis() - status->publishedMs));

    // Chamber: temperature and humidity are null if the reading is not valid.
    const bool chamberValid = status->chamber.valid;
    json.add(",\"chamber\":{\"valid\":%s,\"temperature\":", jsonBool(chamberValid));
    json.addNumber(chamberValid ? status->chamber.temperature : NAN);
    json.add(",\"humidity\":");
    json.addNumber(chamberValid ? status->chamber.humidity : NAN);
    json.add("}");

    json.add(",\"heater\":{\"on\":%s}", jsonBool(status->heater.on));
    json.add(",\"fans\":{\"speed_percent\":%u}", static_cast<unsigned>(status->fans.speedPercent));

    json.add("}");
    return json.ok();
}