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
 
#include "web_server_manager.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <esp_heap_caps.h>

#include "log_json.h"
#include "status_json.h"
#include "../../sysconfig.h"
#include "../utils/logger.h"

namespace {
const char* const WSM_PATH_ROOT      = "/";
const char* const WSM_PATH_INDEX     = "/index";
const char* const WSM_PATH_LOGS      = "/logs";
const char* const WSM_PATH_STATUS    = "/api/status";
const char* const WSM_PATH_LOGS_API  = "/api/logs";

// LittleFS: files of the web interface live in the folder "/www".
const char* const WSM_WWW_DIR        = "/www/";
const char* const WSM_FILE_INDEX     = "/www/index.html";
const char* const WSM_FILE_LOGS      = "/www/logs.html";
const char* const WSM_FS_BASE_PATH   = "/littlefs";
const char* const WSM_FS_PARTITION   = "spiffs";     // label of the file system partition
const uint8_t     WSM_FS_MAX_OPEN    = 10;

// Browsers may keep CSS/JS/images for this long instead of requesting them again
// (less load for the ESP32). Use a small value while developing the pages.
const char* const WSM_CACHE_CONTROL  = "max-age=600";

// Below this amount of free internal RAM, requests are rejected, so that the web
// interface can never use up the memory the rest of the firmware needs.
const size_t WSM_MIN_FREE_HEAP_BYTES = 32768;

const size_t WSM_STATUS_BUFFER_BYTES = 512;   // status JSON is built on the stack
const size_t WSM_LOG_MAX_ENTRIES     = 32;    // per request, about 8 KB of JSON at most

bool memoryLow() {
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < WSM_MIN_FREE_HEAP_BYTES;
}

// Filter for the static file handler: when memory is low, the handler steps aside
// and handleNotFound() answers with 503.
bool memoryOk(AsyncWebServerRequest*) {
    return !memoryLow();
}

void sendBusy(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(503, "text/plain", "Service temporarily unavailable");
    response->addHeader("Retry-After", "5");
    request->send(response);
}

// Value of a numeric query parameter ("?since=12"); 0 if missing or invalid.
uint32_t queryNumber(AsyncWebServerRequest* request, const char* name) {
    const auto* parameter = request->getParam(name);
    if (parameter == nullptr) {
        return 0;
    }
    return static_cast<uint32_t>(strtoul(parameter->value().c_str(), nullptr, 10));
}
}  // namespace

WebServerManager::WebServerManager(const StatusSnapshot& status, uint16_t port)
    : _server(port), _port(port), _status(status) {
}

bool WebServerManager::begin() {
    if (_running) {
        return true;
    }

    // Never format automatically: formatting erases flash, which can stall the
    // whole system for a moment. The image is uploaded from the development PC.
    if (!LittleFS.begin(false, WSM_FS_BASE_PATH, WSM_FS_MAX_OPEN, WSM_FS_PARTITION)) {
        LOGGER_ERROR("Web: LittleFS mount failed (file system image not uploaded?)");
        return false;
    }
    LOGGER_INFO("Web: LittleFS mounted, %lu of %lu bytes used",
               static_cast<unsigned long>(LittleFS.usedBytes()),
               static_cast<unsigned long>(LittleFS.totalBytes()));
    if (!LittleFS.exists(WSM_FILE_INDEX)) {
        LOGGER_WARN("Web: %s not found, upload the data folder", WSM_FILE_INDEX);
    }

    // "Server" header on ALL responses (also static files, redirects, 404/503).
    // DefaultHeaders is global and adds a header per call, so do it only once,
    // even if begin() is called again after end().
    static bool serverHeaderAdded = false;
    if (!serverHeaderAdded) {
        DefaultHeaders::Instance().addHeader("Server", NET_HOSTNAME);
        serverHeaderAdded = true;
    }

    _server.reset();                                     // no duplicate routes after end()/begin()
    _server.on(WSM_PATH_ROOT,     HTTP_GET, handleRoot);
    _server.on(WSM_PATH_INDEX,    HTTP_GET, handleIndex);
    _server.on(WSM_PATH_LOGS,     HTTP_GET, handleLogsPage);
    _server.on(WSM_PATH_LOGS_API, HTTP_GET, handleLogsApi);
    _server.on(WSM_PATH_STATUS,   HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleStatus(request); });

    // Everything else ("/css/style.css", "/js/app.js", ...) comes from the folder /www.
    AsyncStaticWebHandler& files = _server.serveStatic("/", LittleFS, WSM_WWW_DIR);
    files.setCacheControl(WSM_CACHE_CONTROL);
    files.setFilter(memoryOk);

    _server.onNotFound(handleNotFound);
    _server.begin();

    _running = true;
    LOGGER_INFO("Web: server started on port %u", static_cast<unsigned>(_port));
    return true;
}

void WebServerManager::end() {
    if (!_running) {
        return;
    }
    _server.end();
    _running = false;
    LOGGER_INFO("Web: server stopped");
}

bool WebServerManager::isRunning() const {
    return _running;
}

// ---- Request handlers: run in the async_tcp task. Keep them short, never block. ----

void WebServerManager::handleRoot(AsyncWebServerRequest* request) {
    LOGGER_DEBUG("Web: %s %s", request->methodToString(), request->url().c_str());
    request->redirect(WSM_PATH_INDEX);
}

void WebServerManager::handleIndex(AsyncWebServerRequest* request) {
    LOGGER_DEBUG("Web: %s %s", request->methodToString(), request->url().c_str());

    if (memoryLow()) {
        sendBusy(request);
        return;
    }
    request->send(LittleFS, WSM_FILE_INDEX, "text/html");   // streamed from the file, answers 404 if missing
}

void WebServerManager::handleLogsPage(AsyncWebServerRequest* request) {
    LOGGER_DEBUG("Web: %s %s", request->methodToString(), request->url().c_str());

    if (memoryLow()) {
        sendBusy(request);
        return;
    }
    request->send(LittleFS, WSM_FILE_LOGS, "text/html");
}

// Answer: see status_json.cpp.
void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
//    LOGGER_DEBUG("Web: %s %s", request->methodToString(), request->url().c_str());

    if (memoryLow()) {
        sendBusy(request);
        return;
    }

    DeviceStatus status{};
    const bool available = _status.read(status);

    char body[WSM_STATUS_BUFFER_BYTES];
    if (!buildStatusJson(available ? &status : nullptr, body, sizeof body)) {
        LOGGER_ERROR("Web: status JSON does not fit into %u bytes",
                    static_cast<unsigned>(WSM_STATUS_BUFFER_BYTES));
        request->send(500, "text/plain", "Internal Server Error");
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

// GET /api/logs?since=<id>&boot=<boot id>   Answer: see log_json.h.
// No log message is written here, see the class comment.
void WebServerManager::handleLogsApi(AsyncWebServerRequest* request) {
//    LOGGER_DEBUG("Web: %s %s", request->methodToString(), request->url().c_str());

    if (memoryLow()) {
        sendBusy(request);
        return;
    }

    const uint32_t since = queryNumber(request, "since");
    const uint32_t boot  = queryNumber(request, "boot");

    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->addHeader("Cache-Control", "no-store");
    writeLogsJson(*response, boot, since, WSM_LOG_MAX_ENTRIES);
    request->send(response);
}

void WebServerManager::handleNotFound(AsyncWebServerRequest* request) {
   LOGGER_INFO("Web: %s %s --> 404", request->methodToString(), request->url().c_str());

    if (memoryLow()) {
        sendBusy(request);
        return;
    }
    request->send(404, "text/plain", "404 Not Found");
}