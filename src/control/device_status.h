#ifndef DEVICE_STATUS_H
#define DEVICE_STATUS_H

#include <stdint.h>

// Values measured in the drying chamber (SHT4x).
struct ChamberStatus {
    float temperature;   // degrees centigrade
    float humidity;      // percent relative humidity
    bool  valid;         // false: temperature and humidity must not be used
};

// The heater and fan groups are EXAMPLES. Replace the fields by what your
// heater and fan control really provide.
struct HeaterStatus {
    bool on;
};

struct FanStatus {
    uint8_t speedPercent;
};

// Snapshot of the whole device state. Must stay plain data (no String, no pointers):
// it is copied as a whole between tasks.
// Value-initialize it ("DeviceStatus s{};"): everything is then 0/false, i.e. invalid.
struct DeviceStatus {
    ChamberStatus chamber;
    HeaterStatus  heater;
    FanStatus     fans;
    uint32_t      publishedMs;   // millis() at publishing, set by StatusSnapshot::publish()
};

#endif // DEVICE_STATUS_H