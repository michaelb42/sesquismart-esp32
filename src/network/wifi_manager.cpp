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
 
#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>

#include "../../sysconfig.h"

#include "../utils/logger.h"

// secrets.h has to be provided by YOU! 
// Have a look at src/network/secrets.h.example.
#include "secrets.h"           // SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD

namespace {
const uint32_t      WM_CONNECT_TIMEOUT_MS = 10000;   // max. duration of one connection attempt
const uint32_t      WM_RETRY_INTERVAL_MS  = 10000;   // pause after a failed attempt
const uint32_t      WM_CHECK_INTERVAL_MS  = 1000;    // connection check interval while connected
const uint32_t      WM_STOP_TIMEOUT_MS    = WM_CONNECT_TIMEOUT_MS + 2000;   // max. wait in end()
const uint32_t      WM_TASK_STACK_BYTES   = 6144;
const UBaseType_t   WM_TASK_PRIORITY      = tskIDLE_PRIORITY + 1;
const char* const   WM_TASK_NAME          = "wifi";
}  // namespace

bool WifiManager::begin() {
    if (_taskRunning) {
        return true;                 // already running
    }

#if defined(NET_HOSTNAME)
    if (!WiFi.setHostname(NET_HOSTNAME)) {
        LOGGER_WARN("WiFi: could not set hostname");
    }
#endif
    WiFi.persistent(false);          // don't write credentials to flash on every connect
    WiFi.mode(WIFI_STA);

    if (!_apRegistered) {
        _apRegistered = _multi.addAP(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
        if (!_apRegistered) {
            LOGGER_ERROR("WiFi: could not register access point");
            return false;
        }
    }

    _stopRequested = false;
    _taskRunning   = true;           // set before the task exists, so end() never misses it
    const BaseType_t result = xTaskCreate(taskEntry, WM_TASK_NAME, WM_TASK_STACK_BYTES,
                                          this, WM_TASK_PRIORITY, &_task);
    if (result != pdPASS) {
        _taskRunning = false;
        _task = nullptr;
        LOGGER_ERROR("WiFi: could not create task");
        return false;
    }
    return true;
}

void WifiManager::end() {
    if (_taskRunning) {
        _stopRequested = true;
        xTaskNotifyGive(_task);      // wake the task if it is sleeping

        // The task ends itself. Deleting it from outside could interrupt it inside
        // a WiFi call, so wait until it has finished.
        const uint32_t start = millis();
        while (_taskRunning && (millis() - start) < WM_STOP_TIMEOUT_MS) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (_taskRunning) {
            LOGGER_DEBUG("WiFi: task did not stop in time");
            return;                  // call end() again to retry
        }
        _task = nullptr;
    }

    WiFi.disconnect(true);           // true = also switch the WiFi radio off
    _wasConnected = false;
    LOGGER_WARN("WiFi: disconnected");
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::reconnect() {
    if (_taskRunning && _task != nullptr) {
        xTaskNotifyGive(_task);
    }
}

void WifiManager::taskEntry(void* arg) {
    static_cast<WifiManager*>(arg)->run();
}

void WifiManager::run() {
    while (!_stopRequested) {
        if (WiFi.status() != WL_CONNECTED) {
            LOGGER_INFO("WiFi: connecting...");
            _multi.run(WM_CONNECT_TIMEOUT_MS);          // blocks only this task
        }

        const bool connected = (WiFi.status() == WL_CONNECTED);
        trackState(connected);

        // Sleep until the next check, or until reconnect() / end() wakes the task.
        const uint32_t waitMs = connected ? WM_CHECK_INTERVAL_MS : WM_RETRY_INTERVAL_MS;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));
    }

    _taskRunning = false;            // last access to the object, then the task ends itself
    vTaskDelete(nullptr);
}

void WifiManager::trackState(bool connected) {
    if (connected && !_wasConnected) {
        LOGGER_INFO("WiFi: connected to %s, IP %s",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else if (!connected && _wasConnected) {
        LOGGER_WARN("WiFi: connection lost");
    }
    _wasConnected = connected;
}