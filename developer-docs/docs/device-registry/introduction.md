---
title: "Device Registry"
description: "How RADR discovers and creates device instances from BLE service UUIDs"
sidebarTitle: "Device Registry"
---

The device registry is the core system that maps Bluetooth Low Energy (BLE) service UUIDs to device factories. When RADR scans for devices, it uses service UUIDs to determine which factory should create the device instance.

## Architecture Overview

RADR uses a two-tier registry system:

| Tier | Description | Example |
|------|-------------|---------|
| Hardcoded | Devices registered directly in code with custom factories | OSSM |
| Dynamic | Devices loaded from `registry.json` using the ButtplugIO factory | Lovense, Satisfyer, Kiiroo |

```
┌─────────────────────────────────────────────────────────────┐
│                    Device Registry                          │
│                                                             │
│  ┌─────────────────────┐    ┌─────────────────────────────┐│
│  │   Hardcoded Devices │    │      Dynamic Devices        ││
│  │   (Custom Factory)  │    │   (ButtplugIOFactory)       ││
│  │                     │    │                             ││
│  │  Service UUID →     │    │  registry.json →            ││
│  │  Lambda → Device    │    │  Service UUID →             ││
│  │                     │    │  Spec files → Device        ││
│  └─────────────────────┘    └─────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

The registry is populated once at device startup via `initRegistry()`.

## Registry Structure

The registry is a global map that associates service UUIDs with factory functions:

```cpp
std::unordered_map<std::string, DeviceFactory> registry;
```

Where `DeviceFactory` is defined as:

```cpp
typedef Device *(*DeviceFactory)(const NimBLEAdvertisedDevice *advertisedDevice);
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `initRegistry()` | Populates the registry with hardcoded and dynamic devices |
| `getDeviceFactory()` | Looks up a factory by service UUID |

## Hardcoded Devices

Hardcoded devices are registered directly in `initRegistry()` with a lambda factory. This approach is used for devices with custom protocols that require specialized handling.

### OSSM Example

```cpp
registry.emplace(
    OSSM_SERVICE_ID,  // "522B443A-4F53-534D-0001-420BADBABE69"
    [](const NimBLEAdvertisedDevice *advertisedDevice) -> Device * {
        return new OSSM(advertisedDevice);
    });
```

The OSSM uses a custom BLE protocol with multiple characteristics for speed, depth, sensation, and stroke control. This requires a dedicated device class rather than the generic ButtplugIO implementation.

### When to Use Hardcoded Registration

Use hardcoded registration when:

- The device uses a custom protocol not covered by Buttplug.io
- You need specialized UI or control logic
- The device requires unique characteristic handling

## Dynamic Devices (ButtplugIO)

Dynamic devices are loaded from the `data/registry.json` file on the filesystem. This allows adding support for new devices without modifying code.

### registry.json Structure

The registry maps service UUIDs to arrays of protocol spec files:

```json
{
  "0000fff0-0000-1000-8000-00805f9b34fb": ["/protocols/lovense.json"],
  "88f80580-0000-01e6-aace-0002a5d5c51b": ["/protocols/kiiroo-v2.json"],
  "51361500-c5e7-47c7-8a6e-47ebc99d80e8": ["/protocols/satisfyer.json"]
}
```

Each entry maps a BLE service UUID (key) to one or more protocol specification files (value array).

### Factory Flow

When a device with a registered service UUID is discovered:

1. `ButtplugIODeviceFactory` reads `registry.json` from LittleFS
2. Retrieves the list of spec files for the service UUID
3. For each spec file:
   - Loads and parses the JSON configuration
   - Matches the advertised device name against `communication[0].btle.names` patterns
   - Extracts TX/RX characteristics for the service UUID
4. Creates a `LovenseDevice` instance with the matched configuration

### Protocol Spec File Format

Spec files follow the Buttplug.io v4 format:

```json
{
  "defaults": {
    "name": "Lovense Device",
    "features": [...]
  },
  "configurations": [
    {
      "identifier": ["B"],
      "name": "Max",
      "features": [...]
    }
  ],
  "communication": [{
    "btle": {
      "names": ["LVS-*", "LOVE-*"],
      "services": {
        "0000fff0-0000-1000-8000-00805f9b34fb": {
          "tx": "0000fff2-0000-1000-8000-00805f9b34fb",
          "rx": "0000fff1-0000-1000-8000-00805f9b34fb"
        }
      }
    }
  }]
}
```

| Field | Purpose |
|-------|---------|
| `defaults` | Default device name and feature set |
| `configurations` | Device-specific variants and identifiers |
| `communication[].btle.names` | Device name patterns (supports `*` wildcard) |
| `communication[].btle.services` | TX/RX characteristic UUIDs per service |

## Device Discovery Flow

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  BLE Scan    │────▶│  Get Service │────▶│  Lookup in   │────▶│   Factory    │
│  Discovers   │     │    UUID      │     │   Registry   │     │   Creates    │
│   Device     │     │              │     │              │     │   Device     │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
```

1. **Scan**: RADR scans for BLE devices
2. **Extract UUID**: Gets the advertised service UUID
3. **Registry Lookup**: `getDeviceFactory(serviceUUID)` finds the factory
4. **Create Device**: Factory instantiates the appropriate device class

## Adding New Device Support

### Option A: Dynamic (Buttplug.io Registry)

For devices compatible with the Buttplug.io protocol:

### Step 1: Obtain or create the spec file

Get the Buttplug.io v4 protocol specification for the device. These are available in the [Buttplug device config repository](https://github.com/buttplugio/buttplug).

### Step 2: Add spec file to protocols

Place the JSON spec file in `data/protocols/`:
```
data/protocols/your-device.json
```

### Step 3: Update registry.json

Add the service UUID mapping to `data/registry.json`:
```json
{
  "your-service-uuid": ["/protocols/your-device.json"]
}
```

### Step 4: Upload filesystem

Use PlatformIO to upload the filesystem to the device.

### Option B: Hardcoded (Custom Protocol)

For devices requiring custom protocol handling:

### Step 5: Create device class

Create a new device class in `src/devices/` that extends the `Device` base class.

### Step 6: Implement required methods

Implement `getServiceUUID()`, `getName()`, and any device-specific control logic.

### Step 7: Register in initRegistry

Add registration code in `initRegistry()`:
```cpp
registry.emplace(
    YOUR_SERVICE_UUID,
    [](const NimBLEAdvertisedDevice *adv) -> Device * {
        return new YourDevice(adv);
    });
```

### Step 8: Rebuild firmware

Compile and flash the updated firmware.

## Development Workflow

### Updating the Registry (Development)

When developing locally, use PlatformIO's "Upload Filesystem" feature to flash the `data/` directory to LittleFS:

1. Modify `data/registry.json` or add spec files to `data/protocols/`
2. In VS Code with PlatformIO, click **Upload Filesystem** (or run `pio run --target uploadfs`)
3. The device will use the updated registry on next boot

!!! warning
    "Upload Filesystem" erases the existing LittleFS partition before writing. Any runtime modifications will be lost.

### Updating the Registry (Production)

!!! info
    OTA registry updates are a pending feature. Currently, registry updates require a firmware flash.

The planned workflow for production devices:

1. Navigate to **Settings > Look for updates** on the device
2. RADR checks for registry updates from the server
3. Updated `registry.json` and protocol files are downloaded OTA
4. New device support is available without a full firmware update

## Key Source Files

| File | Purpose |
|------|---------|
| `src/devices/registry.h` | Registry interface and type definitions |
| `src/devices/registry.cpp` | Registry implementation and initialization |
| `src/devices/buttplugio/buttplugIOFactory.cpp` | Dynamic device factory |
| `src/devices/device.h` | Base device class |
| `data/registry.json` | Service UUID to spec file mappings |
| `data/protocols/*.json` | Buttplug.io v4 device specifications |

## Further Reading

- **[BLE Communication](/radr/communication/ble)** — Bluetooth protocol details and connection handling.

- **[Buttplug.io Protocol](https://buttplug-spec.docs.buttplug.io/)** — Official Buttplug device protocol specification.
