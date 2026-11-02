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

#include "sht4x_sensor.h"

#include <math.h>

#include "../utils/logger.h"

Sht4xSensor::Sht4xSensor(uint8_t address, TwoWire& wire)
    : _sht4x(address, &wire) {
}

// ---------------------------------------------------------------------------
// Start-up
// ---------------------------------------------------------------------------

bool Sht4xSensor::begin() {
    const uint32_t now = millis();
    _hasValue = false;
    _heaterPending = false;
    _failures = 0;
    _lastError = 0;

    if (!_sht4x.begin()) {  // checks the address and soft-resets the sensor
        _lastError = static_cast<uint8_t>(_sht4x.getError());
        LOGGER_WARN("SHT4x: no answer at I2C address 0x%02X (%s)",
                    static_cast<unsigned>(_sht4x.getAddress()), errorName(_lastError));
        enterRecovery(now);
        return false;
    }

    uint32_t serial = 0;
    if (_sht4x.getSerialNumber(serial)) {
        _serial = serial;
        _hasSerial = true;
        LOGGER_INFO("SHT4x: found at 0x%02X, serial number 0x%08lX",
                    static_cast<unsigned>(_sht4x.getAddress()), static_cast<unsigned long>(_serial));
    } else {
        _hasSerial = false;
        LOGGER_INFO("SHT4x: found at 0x%02X, serial number not readable (%s)",
                    static_cast<unsigned>(_sht4x.getAddress()), errorName(static_cast<uint8_t>(_sht4x.getError())));
    }

    resumeMeasuring(now);
    return true;
}

// ---------------------------------------------------------------------------
// State machine, called from loop()
// ---------------------------------------------------------------------------

void Sht4xSensor::update() {
    const uint32_t now = millis();

    switch (_state) {
        case Sht4xSensorState::Uninitialized:
            break;

        case Sht4xSensorState::Idle:
            if (_heaterPending) {
                if (_sht4x.heatingReady()) {
                    startHeater(now);
                } else {
                    _heaterPending = false;  // the library's overheating protection says no
                }
            } else if (now - _requestMs >= _intervalMs) {
                startMeasurement(now);
            }
            break;

        case Sht4xSensorState::Measuring:
        case Sht4xSensorState::HeaterMeasuring:
            if (_sht4x.dataReady()) {
                finishMeasurement(now);
            } else {
                const uint32_t timeout = (_state == Sht4xSensorState::HeaterMeasuring) ? SHT4X_SENSOR_HEATER_TIMEOUT_MS
                                                                            : SHT4X_SENSOR_TIMEOUT_MS;
                if (now - _requestMs > timeout) {
                    _sht4x.getError();  // dataReady() has no error of its own, keep the last real one
                    _lastError = SHT4x_ERR_READBYTES;
                    fail(now);
                }
            }
            break;

        case Sht4xSensorState::HeaterSettling:
            if (now - _heaterDoneMs >= SHT4X_SENSOR_HEATER_SETTLE_MS) {
                resumeMeasuring(now);
            }
            break;

        case Sht4xSensorState::Recovering:
            if (now - _recoverMs >= SHT4X_SENSOR_RETRY_MS) {
                tryRecover(now);
            }
            break;
    }
}

void Sht4xSensor::startMeasurement(uint32_t now) {
    if (!_sht4x.requestData(precisionCommand(_precision))) {
        _lastError = static_cast<uint8_t>(_sht4x.getError());
        fail(now);
        return;
    }
    _requestMs = now;
    _state = Sht4xSensorState::Measuring;
}

void Sht4xSensor::startHeater(uint32_t now) {
    _heaterPending = false;
    if (!_sht4x.requestData(heaterCommand(_heaterMode))) {
        const uint8_t error = static_cast<uint8_t>(_sht4x.getError());
        if (error == SHT4x_ERR_HEATER_COOLDOWN) {
            return;  // not a sensor fault, the caller can ask again later
        }
        _lastError = error;
        fail(now);
        return;
    }
    _requestMs = now;
    _state = Sht4xSensorState::HeaterMeasuring;
    LOGGER_INFO("SHT4x: heater pulse started");
}

void Sht4xSensor::finishMeasurement(uint32_t now) {
    if (!_sht4x.readData(true)) {  // with CRC check
        _lastError = static_cast<uint8_t>(_sht4x.getError());
        fail(now);
        return;
    }
    _failures = 0;

    if (_state == Sht4xSensorState::HeaterMeasuring) {
        // Measured at the end of the heater pulse: not the chamber air. Discard.
        _heaterDoneMs = now;
        _state = Sht4xSensorState::HeaterSettling;
        LOGGER_INFO("SHT4x: heater pulse done, values invalid for %lu ms",
                    static_cast<unsigned long>(SHT4X_SENSOR_HEATER_SETTLE_MS));
        return;
    }

    _celsius = _sht4x.getCelsius();
    _humidity = _sht4x.getHumidity();
    _lastGoodMs = now;
    _hasValue = true;
    _state = Sht4xSensorState::Idle;
}

void Sht4xSensor::fail(uint32_t now) {
    ++_failureCount;
    ++_failures;
    _heaterPending = false;
    if (_failures >= SHT4X_SENSOR_MAX_FAILURES) {
        LOGGER_WARN("SHT4x: %u failed measurements in a row, last error: %s",
                    static_cast<unsigned>(_failures), errorName(_lastError));
        enterRecovery(now);
    } else {
        _requestMs = now;  // try again after the normal interval
        _state = Sht4xSensorState::Idle;
    }
}

void Sht4xSensor::enterRecovery(uint32_t now) {
    _state = Sht4xSensorState::Recovering;
    _recoverMs = now;
}

void Sht4xSensor::tryRecover(uint32_t now) {
    if (_sht4x.begin()) {  // soft reset, fails if the sensor does not answer
        LOGGER_NOTICE("SHT4x: sensor is back");
        _failures = 0;
        resumeMeasuring(now);
    } else {
        _lastError = static_cast<uint8_t>(_sht4x.getError());
        _recoverMs = now;
    }
}

void Sht4xSensor::resumeMeasuring(uint32_t now) {
    _state = Sht4xSensorState::Idle;
    _requestMs = now - _intervalMs;  // the first measurement is due immediately
}

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

uint32_t Sht4xSensor::ageMs() const {
    return _hasValue ? (millis() - _lastGoodMs) : UINT32_MAX;
}

ChamberStatus Sht4xSensor::chamber() const {
    ChamberStatus c{};
    c.temperature = _celsius;
    c.humidity = _humidity;
    c.valid = _hasValue &&
              _state != Sht4xSensorState::HeaterMeasuring && _state != Sht4xSensorState::HeaterSettling &&
              (millis() - _lastGoodMs) <= SHT4X_SENSOR_STALE_MS;
    return c;
}

float Sht4xSensor::temperatureCelsius() const {
    return _hasValue ? _celsius : NAN;
}

float Sht4xSensor::temperature(TemperatureUnit unit) const {
    return (unit == TemperatureUnit::Fahrenheit) ? temperatureFahrenheit() : temperatureCelsius();
}

float Sht4xSensor::humidity() const {
    return _hasValue ? _humidity : NAN;
}

// ---------------------------------------------------------------------------
// Configuration and heater
// ---------------------------------------------------------------------------

void Sht4xSensor::setIntervalMs(uint32_t intervalMs) {
    _intervalMs = (intervalMs < SHT4X_SENSOR_MIN_INTERVAL_MS) ? SHT4X_SENSOR_MIN_INTERVAL_MS : intervalMs;
}

bool Sht4xSensor::requestHeater(HeaterMode mode) {
    if (_state == Sht4xSensorState::Uninitialized || _state == Sht4xSensorState::Recovering) return false;
    if (isHeaterBusy()) return false;
    if (!_sht4x.heatingReady()) return false;
    _heaterMode = mode;
    _heaterPending = true;
    return true;
}

bool Sht4xSensor::isHeaterBusy() const {
    return _heaterPending || _state == Sht4xSensorState::HeaterMeasuring || _state == Sht4xSensorState::HeaterSettling;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

measType Sht4xSensor::precisionCommand(Precision precision) {
    switch (precision) {
        case Precision::Medium: return SHT4x_MEASUREMENT_MEDIUM;
        case Precision::Low:    return SHT4x_MEASUREMENT_FAST;
        case Precision::High:
        default:                return SHT4x_MEASUREMENT_SLOW;
    }
}

measType Sht4xSensor::heaterCommand(HeaterMode mode) {
    switch (mode) {
        case HeaterMode::HighLong:    return SHT4x_MEASUREMENT_LONG_HIGH_HEAT;
        case HeaterMode::HighShort:   return SHT4x_MEASUREMENT_SHORT_HIGH_HEAT;
        case HeaterMode::MediumLong:  return SHT4x_MEASUREMENT_LONG_MEDIUM_HEAT;
        case HeaterMode::MediumShort: return SHT4x_MEASUREMENT_SHORT_MEDIUM_HEAT;
        case HeaterMode::LowLong:     return SHT4x_MEASUREMENT_LONG_LOW_HEAT;
        case HeaterMode::LowShort:
        default:                      return SHT4x_MEASUREMENT_SHORT_LOW_HEAT;
    }
}

const char* Sht4xSensor::errorName(uint8_t error) {
    switch (error) {
        case SHT4x_OK:                    return "ok";
        case SHT4x_ERR_WRITECMD:          return "command not acknowledged";
        case SHT4x_ERR_READBYTES:         return "no or short answer";
        case SHT4x_ERR_HEATER_OFF:        return "heater off";
        case SHT4x_ERR_NOT_CONNECT:       return "not connected";
        case SHT4x_ERR_CRC_TEMP:          return "CRC error (temperature)";
        case SHT4x_ERR_CRC_HUM:           return "CRC error (humidity)";
        case SHT4x_ERR_CRC_STATUS:        return "CRC error (status)";
        case SHT4x_ERR_HEATER_COOLDOWN:   return "heater cooling down";
        case SHT4x_ERR_HEATER_ON:         return "heater on";
        case SHT4x_ERR_SERIAL_NUMBER_CRC: return "CRC error (serial number)";
        case SHT4x_ERR_INVALID_ADDRESS:   return "invalid I2C address";
        default:                          return "unknown error";
    }
}
