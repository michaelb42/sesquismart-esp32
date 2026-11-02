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
 
#include "ntc_sensor.h"

#include <math.h>

#include "../utils/logger.h"

namespace {
const float NTC_T0_KELVIN = 298.15f;        // 25 degC, where the thermistor has R25
const float NTC_ZERO_CELSIUS_KELVIN = 273.15f;
const float NTC_MIN_CELSIUS = -40.0f;       // outside of this the result cannot be right
const float NTC_MAX_CELSIUS = 200.0f;
}  // namespace

NTCSensor::NTCSensor(uint8_t pin)
    : _pin(pin) {
}

void NTCSensor::begin() {
    pinMode(_pin, INPUT);
    analogSetPinAttenuation(_pin, ADC_11db);    // measuring range up to about 3.1 V
}

float NTCSensor::readCelsius() {
    // 1. Read the voltage at the ADC pin several times and average (the ESP32 ADC is noisy)
    uint32_t sum = 0;
    uint32_t lowest = UINT32_MAX;
    uint32_t highest = 0;
    for (uint8_t i = 0; i < NTC_SENSOR_SAMPLES; ++i) {
        const uint32_t mv = analogReadMilliVolts(_pin);     // uses the calibration of the chip
        sum += mv;
        if (mv < lowest) lowest = mv;
        if (mv > highest) highest = mv;
    }
    const float volts_mv = static_cast<float>(sum) / NTC_SENSOR_SAMPLES;

    // 2. Is the voltage plausible?
    if (volts_mv < NTC_SENSOR_OPEN_MV) {
        setFault(Fault::Open);
        return NAN;
    }
    if (volts_mv >= NTC_SENSOR_OVER_MV) {
        setFault(Fault::OverRange);
        return NAN;
    }
    if (highest - lowest > NTC_SENSOR_MAX_SPREAD_MV) {
        setFault(Fault::Unstable);
        return NAN;
    }

    // 3. Voltage -> thermistor resistance. Divider: V = Vsupply * Rpulldown / (Rntc + Rpulldown)
    const float resistance = NTC_SENSOR_PULLDOWN_OHMS * (NTC_SENSOR_SUPPLY_MV / volts_mv - 1.0f);

    // 4. Resistance -> temperature with the Beta equation: 1/T = 1/T0 + ln(R/R25) / Beta
    const float inverseKelvin = 1.0f / NTC_T0_KELVIN + logf(resistance / NTC_SENSOR_R25_OHMS) / NTC_SENSOR_BETA;
    const float celsius = 1.0f / inverseKelvin - NTC_ZERO_CELSIUS_KELVIN;

    if (!(celsius > NTC_MIN_CELSIUS && celsius < NTC_MAX_CELSIUS)) {    // also catches NAN
        setFault(celsius <= NTC_MIN_CELSIUS ? Fault::Open : Fault::OverRange);
        return NAN;
    }

    setFault(Fault::None);
    return celsius;
}

const char* NTCSensor::faultName(Fault fault) {
    switch (fault) {
        case Fault::None:      return "none";
        case Fault::Open:      return "open circuit";
        case Fault::OverRange: return "short circuit or too hot";
        case Fault::Unstable:  return "unstable reading";
    }
    return "unknown";
}

void NTCSensor::setFault(Fault fault) {
    if (fault == _fault) return;
    if (fault != Fault::None) {
        LOGGER_ERROR("NTC: sensor fault: %s", faultName(fault));
    } else {
        LOGGER_NOTICE("NTC: sensor fault gone (was: %s)", faultName(_fault));
    }
    _fault = fault;
}
