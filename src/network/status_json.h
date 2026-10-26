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
 
#ifndef STATUS_JSON_H
#define STATUS_JSON_H

#include <stddef.h>
#include "../control/device_status.h"

// Writes the status as JSON into buf (no heap, no String).
// status == nullptr means "nothing published yet".
// Returns false if buf is too small; the content is then incomplete.
bool buildStatusJson(const DeviceStatus* status, char* buf, size_t size);

#endif // STATUS_JSON_H