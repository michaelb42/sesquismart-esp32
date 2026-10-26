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

#include "rgb_led.h"

#include <Arduino.h>

namespace {
// Order must match RgbLed::Color.
// nasty dragon: LED_{RED,GREEN,BLUE} are not defines but constexpr
// in the Nano ESP32 board package.
const uint8_t RGB_PINS[3] = { LED_RED, LED_GREEN, LED_BLUE };

// The RGB LED on the Nano ESP32 is common-anode: LOW = on, HIGH = off.
const uint8_t RGB_LEVEL_ON  = LOW;
const uint8_t RGB_LEVEL_OFF = HIGH;

inline uint8_t idx(RgbLed::Color c) { return static_cast<uint8_t>(c); }
}  // namespace

void RgbLed::begin() {
    for (uint8_t i = 0; i < RGB_COLOR_COUNT; ++i) {
        pinMode(RGB_PINS[i], OUTPUT);
        _on[i] = false;
        digitalWrite(RGB_PINS[i], RGB_LEVEL_OFF);
    }
}

void RgbLed::set(Color color) {
    _on[idx(color)] = true;
    apply(color);
}

void RgbLed::unset(Color color) {
    _on[idx(color)] = false;
    apply(color);
}

void RgbLed::toggle(Color color) {
    _on[idx(color)] = !_on[idx(color)];
    apply(color);
}

void RgbLed::unsetAll() {
    unset(Color::Red);
    unset(Color::Green);
    unset(Color::Blue);
}

void RgbLed::setRed()   { set(Color::Red); }
void RgbLed::setGreen() { set(Color::Green); }
void RgbLed::setBlue()  { set(Color::Blue); }

void RgbLed::unsetRed()   { unset(Color::Red); }
void RgbLed::unsetGreen() { unset(Color::Green); }
void RgbLed::unsetBlue()  { unset(Color::Blue); }

void RgbLed::toggleRed()   { toggle(Color::Red); }
void RgbLed::toggleGreen() { toggle(Color::Green); }
void RgbLed::toggleBlue()  { toggle(Color::Blue); }

bool RgbLed::isSet(Color color) const {
    return _on[idx(color)];
}

void RgbLed::apply(Color color) {
    digitalWrite(RGB_PINS[idx(color)], _on[idx(color)] ? RGB_LEVEL_ON : RGB_LEVEL_OFF);
}
