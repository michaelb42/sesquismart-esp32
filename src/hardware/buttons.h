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
 
#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

// Reads the three HMI push buttons and detects short and long presses.
//  - Short press: released before 2000 ms. Reported on release.
//  - Long press:  held for at least 2000 ms. Reported once, as soon as the
//                 time is reached (while the button is still held).
// A single press produces either a short or a long event, never both.
class Buttons {
public:
    enum class Button : uint8_t { BUTTON_PLUS = 0, BUTTON_MINUS = 1, ACTION_BUTTON = 2 };

    Buttons() = default;

    void begin();                          // call once in setup()
    void update();                         // call often in loop(), never blocks

    bool isPressed(Button button) const;   // debounced current state

    // Each returns true once per event and then clears it.
    bool wasShortPressed(Button button);
    bool wasLongPressed(Button button);

private:
    static constexpr uint8_t BTN_COUNT = 3;

    struct State {
        bool     rawPressed    = false;    // last raw reading
        bool     stablePressed = false;    // debounced state
        uint32_t lastChangeMs  = 0;        // time of the last raw change
        bool     longReported  = false;    // long event already fired for this press
        bool     shortEvent    = false;    // pending, not yet consumed
        bool     longEvent     = false;    // pending, not yet consumed
    };

    State _state[BTN_COUNT];
};

#endif  // BUTTONS_H