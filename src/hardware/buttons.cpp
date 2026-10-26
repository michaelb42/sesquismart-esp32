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
 
#include "buttons.h"

#include <esp32-hal-gpio.h> // for ESP32 INPUT_PULLDOWN
#include <Arduino.h>

#include "../../sysconfig.h"     // defines PIN_BUTTON_PLUS, PIN_BUTTON_MINUS, PIN_ACTION_BUTTON

namespace {
// Order must match Buttons::Button.
const uint8_t BTN_PINS[3] = { PIN_BUTTON_PLUS, PIN_BUTTON_MINUS, PIN_ACTION_BUTTON };

// Wiring: button between pin and 5 V,limited to 3.0V on pigyback, external pull-down to GND -> pressed = HIGH.
const uint8_t  BTN_PIN_MODE     = INPUT_PULLDOWN; // do not float the pins when HMI is not connected
const uint8_t  BTN_LEVEL_ACTIVE = HIGH;

const uint32_t BTN_DEBOUNCE_MS   = 30;
const uint32_t BTN_LONG_PRESS_MS = 2000;

inline uint8_t idx(Buttons::Button b) { return static_cast<uint8_t>(b); }
}  // namespace

void Buttons::begin() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        pinMode(BTN_PINS[i], BTN_PIN_MODE);
        _state[i] = State();
        _state[i].rawPressed = (digitalRead(BTN_PINS[i]) == BTN_LEVEL_ACTIVE);
        _state[i].stablePressed = _state[i].rawPressed;
        _state[i].lastChangeMs = now;
        _state[i].longReported = _state[i].stablePressed;  // a button held at boot gives no event
    }
}

void Buttons::update() {
    const uint32_t now = millis();

    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
        State& s = _state[i];
        const bool raw = (digitalRead(BTN_PINS[i]) == BTN_LEVEL_ACTIVE);

        if (raw != s.rawPressed) {                 // bounce or real edge: restart debounce timer
            s.rawPressed = raw;
            s.lastChangeMs = now;
        }

        // Accept the new state once it has been stable long enough.
        if (s.rawPressed != s.stablePressed && (now - s.lastChangeMs) >= BTN_DEBOUNCE_MS) {
            s.stablePressed = s.rawPressed;
            if (s.stablePressed) {
                s.longReported = false;            // new press starts
            } else if (!s.longReported) {
                s.shortEvent = true;               // released before the long-press time
            }
        }

        // Long press: fire once when the hold time is reached.
        if (s.stablePressed && !s.longReported && (now - s.lastChangeMs) >= BTN_LONG_PRESS_MS) {
            s.longReported = true;
            s.longEvent = true;
        }
    }
}

bool Buttons::isPressed(Button button) const {
    return _state[idx(button)].stablePressed;
}

bool Buttons::wasShortPressed(Button button) {
    State& s = _state[idx(button)];
    const bool result = s.shortEvent;
    s.shortEvent = false;
    return result;
}

bool Buttons::wasLongPressed(Button button) {
    State& s = _state[idx(button)];
    const bool result = s.longEvent;
    s.longEvent = false;
    return result;
}