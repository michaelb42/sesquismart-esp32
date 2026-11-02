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

#ifndef SHT4X_SENSOR_H
#define SHT4X_SENSOR_H

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>
#include <SHT4x.h>          // library "SHT4x" by Rob Tillaart (Library Manager)

#include "../../sysconfig.h"
#include "../utils/temp_conv.h"
#include "../control/device_status.h"

// ---------------------------------------------------------------------------
// Configuration. The defaults below can be overridden in sysconfig.h.
// ---------------------------------------------------------------------------

// 7-bit I2C address of the sensor (0x44 for most SHT4x types, see the datasheet)
#ifndef SHT4X_SENSOR_ADDRESS
#define SHT4X_SENSOR_ADDRESS 0x44
#endif

// Time between two measurements (ms)
#ifndef SHT4X_SENSOR_INTERVAL_MS
#define SHT4X_SENSOR_INTERVAL_MS 2000
#endif

// The values count as invalid when the last good measurement is older than this (ms)
#ifndef SHT4X_SENSOR_STALE_MS
#define SHT4X_SENSOR_STALE_MS (6 * SHT4X_SENSOR_INTERVAL_MS)
#endif

// This many failed measurements in a row put the sensor into recovery mode
#ifndef SHT4X_SENSOR_MAX_FAILURES
#define SHT4X_SENSOR_MAX_FAILURES 3
#endif

// In recovery mode the sensor is reset and probed again at this interval (ms)
#ifndef SHT4X_SENSOR_RETRY_MS
#define SHT4X_SENSOR_RETRY_MS 5000
#endif

// After a heater pulse the sensor is warm. No measurement is taken and the values
// stay invalid for this time (ms). Conservative default, tune it with the datasheet
// and Sensirion's application note on the integrated heater.
#ifndef SHT4X_SENSOR_HEATER_SETTLE_MS
#define SHT4X_SENSOR_HEATER_SETTLE_MS 5000
#endif

// Non-blocking driver for the SHT4x temperature and humidity sensor in the drying
// chamber. A thin layer on top of Rob Tillaart's SHT4x library that uses only its
// asynchronous interface: update() sends the measurement command, returns
// immediately and collects the result later, so loop() is never stalled by the
// 2 .. 9 ms (and, for the heater, 110 / 1100 ms) the sensor needs.
//
// Temperatures are stored in degrees Celsius and, converted from the raw sensor
// value, also in degrees Fahrenheit. The device status (ChamberStatus) is always
// in Celsius; Fahrenheit is meant for display and web output.
//
// All methods must be called from the same task (loop()), which also owns the other
// devices on the shared I2C bus (display). Never call it from web server handlers;
// they read the published DeviceStatus instead.

class Sht4xSensor {
public:
    // Measurement repeatability: duration 9 / 5 / 2 ms
    enum class Precision : uint8_t { High, Medium, Low };

    // Heater pulses: power 200 / 110 / 20 mW, long = 1 s, short = 0.1 s.
    // The sensor measures at the end of the pulse; the values are not published.
    enum class HeaterMode : uint8_t { HighLong, HighShort, MediumLong, MediumShort, LowLong, LowShort };

    enum class Sht4xSensorState : uint8_t {
        Uninitialized,      // begin() not called yet
        Idle,               // waiting for the next measurement
        Measuring,          // command sent, waiting for the result
        HeaterMeasuring,    // heater pulse running
        HeaterSettling,     // pulse done, sensor cooling down
        Recovering          // sensor does not answer, retrying periodically
    };

    explicit Sht4xSensor(uint8_t address = SHT4X_SENSOR_ADDRESS, TwoWire& wire = Wire);

    Sht4xSensor(const Sht4xSensor&) = delete;
    Sht4xSensor& operator=(const Sht4xSensor&) = delete;

    // Call once in setup(). Resets the sensor and reads its serial number.
    // Returns false if the sensor does not answer; update() keeps trying then.
    bool begin();

    // Call often in loop(), never blocks.
    void update();

    // --- Measured values ------------------------------------------------------

    // For DeviceStatus::chamber: always degrees Celsius. valid is false until the
    // first measurement, when the last good one is older than SHT4X_SENSOR_STALE_MS
    // and while a heater pulse is running or the sensor is cooling down.
    ChamberStatus chamber() const;

    bool    hasValue() const { return _hasValue; }              // at least one good measurement so far
    uint32_t ageMs() const;                                      // time since the last good measurement

    // NAN as long as there is no measurement. Check chamber().valid before using them.
    float temperatureCelsius() const;
    float temperatureFahrenheit() const;
    float temperature(TemperatureUnit unit) const;
    float temperature() const { return temperature(_unit); }   // in the configured unit
    float humidity() const;                                      // % relative humidity, 0..100

    void setTemperatureUnit(TemperatureUnit unit) { _unit = unit; }
    TemperatureUnit temperatureUnit() const { return _unit; }

    // --- Configuration --------------------------------------------------------

    void setPrecision(Precision precision) { _precision = precision; }
    Precision precision() const { return _precision; }

    // Time between measurements, at least SHT4X_SENSOR_MIN_INTERVAL_MS.
    void setIntervalMs(uint32_t intervalMs);
    uint32_t intervalMs() const { return _intervalMs; }

    // --- Heater -----------------------------------------------------------------

    // Schedules a heater pulse that starts after the running measurement. Returns false
    // if it is not possible right now: sensor not working, a pulse already scheduled or
    // running, or the sensor's overheating protection (duty cycle of the library) says
    // "too early". Ask again later in that case.
    bool requestHeater(HeaterMode mode);
    bool isHeaterBusy() const;      // pulse scheduled, running or sensor still cooling down

    // --- Diagnostics ------------------------------------------------------------

    Sht4xSensorState state() const { return _state; }
    bool hasSerialNumber() const { return _hasSerial; }
    uint32_t serialNumber() const { return _serial; }
    uint8_t lastError() const { return _lastError; }            // SHT4x_ERR_* of the library, 0 = none
    uint32_t failureCount() const { return _failureCount; }     // failed measurements since begin()
    static const char* errorName(uint8_t error);

private:
    static constexpr uint32_t SHT4X_SENSOR_MIN_INTERVAL_MS = 100;
    static constexpr uint32_t SHT4X_SENSOR_TIMEOUT_MS = 100;           // result of a normal measurement
    static constexpr uint32_t SHT4X_SENSOR_HEATER_TIMEOUT_MS = 1500;   // result of a heater pulse

    void startMeasurement(uint32_t now);
    void startHeater(uint32_t now);
    void finishMeasurement(uint32_t now);
    void fail(uint32_t now);
    void enterRecovery(uint32_t now);
    void tryRecover(uint32_t now);
    void resumeMeasuring(uint32_t now);
    static measType precisionCommand(Precision precision);
    static measType heaterCommand(HeaterMode mode);

    SHT4x           _sht4x;
    TemperatureUnit _unit = TemperatureUnit::Celsius;
    Precision       _precision = Precision::High;
    uint32_t        _intervalMs = SHT4X_SENSOR_INTERVAL_MS;

    Sht4xSensorState    _state = Sht4xSensorState::Uninitialized;
    uint32_t _requestMs = 0;        // when the running / last measurement was requested
    uint32_t _heaterDoneMs = 0;
    uint32_t _recoverMs = 0;
    bool     _heaterPending = false;
    HeaterMode _heaterMode = HeaterMode::HighShort;

    bool     _hasValue = false;
    uint32_t _lastGoodMs = 0;
    float    _celsius = 0.0f;
    float    _fahrenheit = 0.0f;
    float    _humidity = 0.0f;

    bool     _hasSerial = false;
    uint32_t _serial = 0;
    uint8_t  _lastError = 0;
    uint8_t  _failures = 0;         // in a row
    uint32_t _failureCount = 0;     // total
};

#endif // SHT4X_SENSOR_H
