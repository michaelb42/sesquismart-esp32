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
 
#ifndef NTC_SENSOR_H
#define NTC_SENSOR_H

#include <stdint.h>
#include <Arduino.h>

#include "../../sysconfig.h"

// ---------------------------------------------------------------------------
// Configuration. The defaults below can be overridden in sysconfig.h.
// ---------------------------------------------------------------------------

// Analog input of the thermistor. Must be an ADC1 pin: ADC2 cannot be used while WiFi
// is on. A3 is GPIO4 = ADC1 channel 3 on the Nano ESP32.
#ifndef PIN_NTC_SENSOR
#define PIN_NTC_SENSOR A3
#endif

// The NTC Thermistor used in the heater is a generic 100K type, no part number
// is given. Resistance at 25 degC and its Beta value (B25/85 in the data sheet).
// 100 kOhm / 3950 K is typical for generic 100k thermistors; use the values of your
// type if you know them. The error is about 1 degC for a wrong Beta of +-100 K.
#ifndef NTC_SENSOR_R25_OHMS
#define NTC_SENSOR_R25_OHMS 100000.0f
#endif
#ifndef NTC_SENSOR_BETA
#define NTC_SENSOR_BETA 3950.0f
#endif

// The voltage divider on the mainboard: NTC from +5V to the sense node, R21 from the
// node to GND. The 5V rail is assumed to be exactly 5.0 V: a rail that is 2 % off causes
// a measurement error of about 0.5 degC at 65 degC.
#ifndef NTC_SENSOR_PULLDOWN_OHMS
#define NTC_SENSOR_PULLDOWN_OHMS 10000.0f
#endif
#ifndef NTC_SENSOR_SUPPLY_MV
#define NTC_SENSOR_SUPPLY_MV 5000.0f
#endif

// Number of ADC readings that are averaged for one measurement
#ifndef NTC_SENSOR_SAMPLES
#define NTC_SENSOR_SAMPLES 16
#endif

// Plausibility limits (voltage at the ADC pin), see Fault below
#ifndef NTC_SENSOR_OPEN_MV
#define NTC_SENSOR_OPEN_MV 185          // below: thermistor missing / wire broken (colder than about 5 degC)
#endif
#ifndef NTC_SENSOR_OVER_MV
#define NTC_SENSOR_OVER_MV 2007         // above: thermistor shorted or hotter than the device allows (about 75 degC)
#endif
#ifndef NTC_SENSOR_MAX_SPREAD_MV
#define NTC_SENSOR_MAX_SPREAD_MV 250    // difference between the highest and lowest of the readings
#endif

// Reads the NTC thermistor that watches the heater in the drying chamber. It is the
// safety device that makes sure the heater cannot overheat, so the class does not only
// measure but also checks that the measurement makes sense.
//
// Circuit (pins and parts as on the mainboard and the piggyback board):
//
//   +5V ---[ NTC 100k ]---+---[ 4.7k ]---+--- A3 (GPIO4, ADC1 channel 3)
//                         |              |
//                       [10k]  [1u]    [100n]      (10k = R21, 1u = C7 on the mainboard,
//                         |      |        |         4.7k and 100n on the piggyback,
//                        GND    GND      GND        plus a Schottky diode from A3 to 3V3)
//
// Usage: call begin() once in setup(). While drying is active call readCelsius() from
// loop() about every 10 seconds (e.g. with a millis() check). A call takes about a
// millisecond, never blocks longer and can be done from loop() at any time.
//
//   float t = ntc.readCelsius();
//   if (!(t < MAX_HEATER_TEMPERATURE)) { /* too hot OR sensor fault: heater off! */ }
//
// Note the form of the comparison: NAN (a sensor fault) is neither smaller nor greater
// than anything, so "t > limit" would be false for a faulty sensor, while "!(t < limit)"
// is true. Use fault() to find out why the value is NAN.
//
// Not thread-safe: use it only from loop().
class NTCSensor {
public:
    enum class Fault : uint8_t {
        None,           // measurement is good
        Open,           // low voltage: thermistor missing or wire broken
        OverRange,      // voltage too high: thermistor shorted or too hot to measure
        Unstable        // the readings jump around: bad contact or disturbance
    };

    // pin: analog input of the thermistor, should be an ADC1 pin for an ESP32.
    explicit NTCSensor(uint8_t pin = PIN_NTC_SENSOR);

    // Call once in setup(): configures the analog input.
    void begin();

    // Measures the temperature in degrees Celsius. Returns NAN if the measurement is not
    // trustworthy; fault() tells why. A fault is logged once when it appears and again
    // when it is gone.
    float readCelsius();

    // Result of the last readCelsius() call, None before the first call.
    Fault fault() const { return _fault; }

    // Name of a fault for logs and the web interface.
    static const char* faultName(Fault fault);

private:
    void setFault(Fault fault);

    uint8_t _pin;
    Fault   _fault = Fault::None;
};

#endif // NTC_SENSOR_H
