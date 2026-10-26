#include "status_snapshot.h"

#include <Arduino.h>

namespace {
const uint8_t SNAP_MAX_READ_TRIES = 4;
}

void StatusSnapshot::publish(const DeviceStatus& status) {
    DeviceStatus copy = status;
    copy.publishedMs = millis();

    const uint32_t seq = _seq.load(std::memory_order_relaxed);

    _seq.store(seq + 1, std::memory_order_relaxed);        // odd: write in progress
    std::atomic_thread_fence(std::memory_order_release);

    _data = copy;

    _seq.store(seq + 2, std::memory_order_release);        // even: consistent again
}

bool StatusSnapshot::read(DeviceStatus& out) const {
    for (uint8_t i = 0; i < SNAP_MAX_READ_TRIES; i++) {
        const uint32_t before = _seq.load(std::memory_order_acquire);
        if (before == 0) {
            return false;                                  // nothing published yet
        }
        if (before & 1U) {
            continue;                                      // write in progress, try again
        }

        const DeviceStatus copy = _data;                   // may be torn, checked below

        std::atomic_thread_fence(std::memory_order_acquire);
        if (_seq.load(std::memory_order_relaxed) == before) {
            out = copy;
            return true;
        }
    }
    return false;                                          // never wait
}