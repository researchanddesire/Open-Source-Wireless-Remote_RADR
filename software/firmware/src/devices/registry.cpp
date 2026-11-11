#include "registry.h"
#include <Arduino.h>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <string>

#include "buttplugio/buttplugIOFactory.h"
#include "buttplugio/utils.h"
#include "lovense/LovenseDevice.hpp"
#include "lovense/data.hpp"
#include "lovense/domi/domi_device.hpp"
#include "researchAndDesire/ossm/ossm_device.hpp"
#include "serviceUUIDs.h"

static const char *REGISTRY_TAG = "REGISTRY";

// The registry is one table instead of a map. A map of 92 UUID strings took
// about 10 KB of internal RAM in roughly 180 small blocks, which RADR cannot
// spare: once Wi-Fi and BLE are up, internal RAM decides whether the BLE
// connection task can be created at all. The table is a single block that
// lives in PSRAM (internal RAM only when the board has no PSRAM).
namespace {
constexpr size_t kUuidLength = 36;

struct RegistryEntry {
    char uuid[kUuidLength + 1];  // upper case
    DeviceFactory factory;
};

RegistryEntry *entries = nullptr;
size_t entryCount = 0;
size_t entryCapacity = 0;

Device *createOssm(const NimBLEAdvertisedDevice *advertisedDevice) {
    return new OSSM(advertisedDevice);
}

// Services with their own device class. They are added before registry.json,
// so a file entry can never replace them.
struct ExplicitService {
    const char *uuid;
    DeviceFactory factory;
};
const ExplicitService kExplicitServices[] = {
    {OSSM_SERVICE_ID, createOssm},
};

const DeviceFactory *findEntry(const char *upperCaseUuid) {
    for (size_t i = 0; i < entryCount; i++) {
        if (strcmp(upperCaseUuid, entries[i].uuid) == 0) {
            return &entries[i].factory;
        }
    }
    return nullptr;
}

// The first entry for a UUID wins, as it did with the map.
void addEntry(const char *uuid, DeviceFactory factory) {
    const size_t length = strlen(uuid);
    if (entryCount >= entryCapacity || length == 0 || length > kUuidLength) {
        ESP_LOGW(REGISTRY_TAG, "Skipped registry entry: %s", uuid);
        return;
    }
    RegistryEntry &entry = entries[entryCount];
    for (size_t i = 0; i < length; i++) {
        entry.uuid[i] = static_cast<char>(toupper(uuid[i]));
    }
    entry.uuid[length] = '\0';
    if (findEntry(entry.uuid) != nullptr) return;
    entry.factory = factory;
    entryCount++;
}

bool allocateEntries(size_t capacity) {
    heap_caps_free(entries);
    entryCount = 0;
    entryCapacity = 0;
    entries = static_cast<RegistryEntry *>(heap_caps_malloc_prefer(
        capacity * sizeof(RegistryEntry), 2,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT));
    if (entries == nullptr) {
        ESP_LOGE(REGISTRY_TAG, "No memory for %u registry entries",
                 static_cast<unsigned>(capacity));
        return false;
    }
    entryCapacity = capacity;
    return true;
}
}  // namespace

void initRegistry() {
    // Large and short-lived: keep the parsed file out of internal RAM too.
    JsonDocument doc(buttplugJsonAllocator());
    bool loaded = false;

    if (LittleFS.exists("/registry.json")) {
        ESP_LOGI(REGISTRY_TAG, "registry.json exists");
        File file = LittleFS.open("/registry.json", "r");
        if (file) {
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            if (error) {
                ESP_LOGW(REGISTRY_TAG, "Failed to parse registry.json: %s",
                         error.c_str());
            } else {
                loaded = true;
                ESP_LOGI(REGISTRY_TAG, "Loaded device registry from LittleFS");
            }
        } else {
            ESP_LOGW(REGISTRY_TAG, "Failed to open registry.json");
        }
    } else {
        ESP_LOGD(REGISTRY_TAG, "registry.json not found in LittleFS");
    }

    JsonObjectConst services = doc.as<JsonObjectConst>();
    const size_t explicitCount =
        sizeof(kExplicitServices) / sizeof(kExplicitServices[0]);
    if (!allocateEntries(explicitCount + (loaded ? services.size() : 0))) {
        return;
    }

    for (const ExplicitService &service : kExplicitServices) {
        addEntry(service.uuid, service.factory);
    }

    if (loaded) {
        for (JsonPairConst pair : services) {
            ESP_LOGD(REGISTRY_TAG, "UUID: %s", pair.key().c_str());
            addEntry(pair.key().c_str(), ButtplugIODeviceFactory);
        }
    }
    ESP_LOGI(REGISTRY_TAG, "Device registry holds %u services",
             static_cast<unsigned>(entryCount));
}

const DeviceFactory *getDeviceFactory(const NimBLEUUID &serviceUUID) {
    // Registry keys are the upper-case form of NimBLEUUID::toString().
    std::string uuidStr = serviceUUID.toString();
    std::transform(uuidStr.begin(), uuidStr.end(), uuidStr.begin(), ::toupper);

    const DeviceFactory *factory = findEntry(uuidStr.c_str());
    if (factory != nullptr) return factory;

    ESP_LOGD(REGISTRY_TAG, "No device factory found for service UUID: %s",
             uuidStr.c_str());
    return nullptr;
}

size_t getDeviceRegistrySize() { return entryCount; }

const char *getDeviceRegistryUuid(size_t index) {
    return index < entryCount ? entries[index].uuid : nullptr;
}
