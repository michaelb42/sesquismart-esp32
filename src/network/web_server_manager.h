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
 
#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <stdint.h>
#include <ESPAsyncWebServer.h>

#include "../control/status_snapshot.h"

// HTTP server for the web interface (ESPAsyncWebServer + AsyncTCP).
// HTML, CSS, JavaScript and other files are read from the LittleFS folder "/www".
//   /index       page with the device state     (/www/index.html)
//   /logs        page with the log buffer        (/www/logs.html)
//   /api/status  device state as JSON
//   /api/logs    buffered log entries as JSON
//
// Rules that keep the web interface from affecting sensors or safety functions:
//  - This class never calls sensor, control or safety code. Its only connections to the
//    rest of the firmware are the read-only StatusSnapshot and the log buffer.
//  - Request handlers run in the library's own task (async_tcp), never in loop().
//    They only read files/snapshots and never block or wait for anything else.
//  - The file system is only read, never written or formatted, so the web interface
//    causes no flash erase/write operations.
//  - When internal RAM runs low, requests are answered with "503 Service Unavailable".
//  - The handlers of /api/status and /api/logs never write log messages themselves
//    (otherwise every poll of the log page would create new log entries).
class WebServerManager {
public:
    static constexpr uint16_t WSM_DEFAULT_PORT = 80;

    explicit WebServerManager(const StatusSnapshot& status, uint16_t port = WSM_DEFAULT_PORT);

    WebServerManager(const WebServerManager&) = delete;
    WebServerManager& operator=(const WebServerManager&) = delete;

    // Mounts LittleFS, registers the routes and starts listening. Call after
    // WifiManager::begin(), so that the network stack is initialized.
    // Returns false (and does not start the server) if the file system cannot be
    // mounted, e.g. because no file system image was uploaded yet.
    // May be called again after end().
    bool begin();

    // Stops listening and removes the routes. LittleFS stays mounted, because other
    // components may use it.
    void end();

    bool isRunning() const;

private:
    static void handleRoot(AsyncWebServerRequest* request);
    static void handleIndex(AsyncWebServerRequest* request);
    static void handleLogsPage(AsyncWebServerRequest* request);
    static void handleLogsApi(AsyncWebServerRequest* request);
    static void handleNotFound(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);

    AsyncWebServer        _server;
    uint16_t              _port;
    const StatusSnapshot& _status;
    bool                  _running = false;
};

#endif // WEB_SERVER_MANAGER_H