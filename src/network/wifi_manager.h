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
 
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <WiFiMulti.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Keeps the Nano ESP32 connected to WiFi using the credentials from secrets.h.
// A FreeRTOS task does all connecting and reconnecting in the background, so the
// blocking WiFiMulti calls never stall loop().
class WifiManager {
public:
    WifiManager() = default;

    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    // Call once in setup(): configures station mode, registers the access point(s)
    // and starts the WiFi task. Returns false if that failed.
    // May be called again after end().
    bool begin();

    // Stops the WiFi task, disconnects from the access point and switches the
    // WiFi radio off. Blocks until the task has finished; if it is just in the
    // middle of a connection attempt this can take up to about 10 seconds.
    void end();

    // Current connection state (safe to call from any task, e.g. loop()).
    bool isConnected() const;

    // Optional: wakes the WiFi task for an immediate check/connection attempt
    // instead of waiting for its next scheduled one. Returns immediately.
    // Do not call it concurrently with end().
    void reconnect();

private:
    static void taskEntry(void* arg);   // FreeRTOS entry point
    void run();                         // task body, ends itself when stop is requested
    void trackState(bool connected);    // logs connect/disconnect transitions

    WiFiMulti     _multi;
    TaskHandle_t  _task          = nullptr;
    volatile bool _taskRunning   = false;
    volatile bool _stopRequested = false;
    bool          _apRegistered  = false;   // access point is added to WiFiMulti only once
    bool          _wasConnected  = false;   // only touched by the WiFi task (and end(), after it stopped)
};

#endif // WIFI_MANAGER_H