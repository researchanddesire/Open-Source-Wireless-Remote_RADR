### Devices Module

Technical documentation for developers adding new BLE devices to the remote.

## Supported Devices

- **Research And Desire**
  - **OSSM**: Supported
  - **LKBX**: Pending
  - **DTT**: Pending
- **Lovense**
  - **Domi 2**: Pending

## Registry Overview

Devices are discovered by their primary service UUID and instantiated via a factory registry.

- Registry map: `src/devices/registry.h` and `src/devices/registry.cpp`
- Known service UUIDs: `src/devices/serviceUUIDs.h`
- Device base class: `src/devices/device.h`

## How to Add a Device to the Registry

1. **Obtain the service UUID**

- Use a Bluetooth sniffing/inspection tool (e.g., Bluetility on macOS) to find the device's primary service UUID.

2. **Add the service UUID to `serviceUUIDs.h`**

- Store the UUID as a PROGMEM string constant.

```cpp
// src/devices/serviceUUIDs.h
static const char NEW_DEVICE_SERVICE_ID[] PROGMEM = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
```

3. **Register a factory in `registry.cpp`**

- Include your device header and add an entry in the `initRegistry()` function mapping the service UUID to a factory that returns a `Device*` of your concrete class.

```cpp
// src/devices/registry.cpp
#include "your_vendor/your_device/your_device.h"

void initRegistry() {
    registry.clear();
    
    registry.emplace(OSSM_SERVICE_ID, [](const NimBLEAdvertisedDevice *ad) -> Device* { return new OSSM(ad); });
    registry.emplace(DOMI_SERVICE_ID, [](const NimBLEAdvertisedDevice *ad) -> Device* { return new Domi2(ad); });
    registry.emplace(NEW_DEVICE_SERVICE_ID, [](const NimBLEAdvertisedDevice *ad) -> Device* { return new YourDeviceClass(ad); });
    
    // ... rest of LittleFS loading ...
}
```

**Note:** Call `initRegistry()` once during startup (e.g., in `setup()`) before using the registry.

4. **Provide a concrete `Device` implementation**

- Create a header/implementation pair for your device that derives from `Device` (see `src/devices/device.h`).
- **Preferred file structure:**  
  Place your files in  
  `/src/devices/<brand_name>/<device_name>/<device>_device.h`  
  `/src/devices/<brand_name>/<device_name>/<device>_device.cpp`  
  For example, for a "FooBar" device from "Acme", use:  
  `/src/devices/acme/foobar/foobar_device.h`  
  `/src/devices/acme/foobar/foobar_device.cpp`

- Implement input handlers, drawing, and value sync methods as needed.
