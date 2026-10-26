#ifndef STATUS_SNAPSHOT_H
#define STATUS_SNAPSHOT_H

#include <stdint.h>
#include <atomic>
#include <type_traits>

#include "device_status.h"

// Read-only copy of the device state for the web interface.
//
// Exactly ONE task (loop()) publishes, any number of readers (async_tcp) read.
// Neither side ever waits for the other (seqlock): the writer is never blocked
// by a reader, and a reader gives up after a few tries instead of waiting.
// The whole DeviceStatus is copied at once, so a reader never sees a mix of
// old and new values.
class StatusSnapshot {
public:
    // Call only from the task that owns the device state, e.g. loop().
    void publish(const DeviceStatus& status);

    // Returns false if nothing was published yet (or, extremely rarely, if a
    // consistent copy could not be read within a few tries).
    bool read(DeviceStatus& out) const;

private:
    static_assert(std::is_trivially_copyable<DeviceStatus>::value,
                  "DeviceStatus must be plain data");

    std::atomic<uint32_t> _seq{0};   // 0 = never published, odd = write in progress
    DeviceStatus          _data{};
};

#endif // STATUS_SNAPSHOT_H