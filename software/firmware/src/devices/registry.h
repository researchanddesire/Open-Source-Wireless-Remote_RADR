#ifndef REGISTRY_H
#define REGISTRY_H

#include <NimBLEUUID.h>

#include "device.h"

// Factory function type for creating device instances
typedef Device *(*DeviceFactory)(
    const NimBLEAdvertisedDevice *advertisedDevice);

// Initialize the device registry (loads from LittleFS)
void initRegistry();

// Look up a device factory by service UUID
const DeviceFactory *getDeviceFactory(const NimBLEUUID &serviceUUID);

// Number of registered services, and the upper-case UUID of one of them
// (nullptr when the index is out of range).
size_t getDeviceRegistrySize();
const char *getDeviceRegistryUuid(size_t index);

#endif
