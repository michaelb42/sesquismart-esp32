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
 
#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>

// Controls the RGB LED of the Arduino Nano ESP32.
// Each color (red, green, blue) can be switched on, off or toggled individually.
class RgbLed {
public:
    enum class Color : uint8_t { Red = 0, Green = 1, Blue = 2 };

    RgbLed() = default;

    void begin();               // call once in setup(), before any other method

    // Generic methods
    void set(Color color);      // switch the color on
    void unset(Color color);    // switch the color off
    void toggle(Color color);   // toggle the color's current state
    void unsetAll();            // all three colors off

    // Shortcuts
    void setRed();
    void setGreen();
    void setBlue();

    void unsetRed();
    void unsetGreen();
    void unsetBlue();

    void toggleRed();
    void toggleGreen();
    void toggleBlue();

    bool isSet(Color color) const;

private:
    static constexpr uint8_t RGB_COLOR_COUNT = 3;

    void apply(Color color);    // writes the stored state to the pin

    bool _on[RGB_COLOR_COUNT] = { false, false, false };
};

#endif // RGB_LED_H