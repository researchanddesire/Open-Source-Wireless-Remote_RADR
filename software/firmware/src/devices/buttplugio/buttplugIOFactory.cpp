#include "buttplugIOFactory.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils.h"

namespace {

/// @brief Finds the protocol file and BLE characteristics for a device.
///
/// The registry and protocol files are parsed into PSRAM and released before
/// this returns. A device starts its connection task as soon as it is
/// constructed, and that task's stack must come from internal RAM, so no
/// large document may still be alive when the device is created.
///
/// @param serviceUUID The advertised service UUID
/// @param deviceName The advertised device name
/// @param configFileName Receives the matching protocol file name
/// @param characteristics Receives a copy of the service's characteristics
/// @return true if a matching configuration was found
bool findConfiguration(const std::string& serviceUUID,
                       const String& deviceName, String& configFileName,
                       JsonDocument& characteristics) {
    JsonDocument registryDoc(buttplugJsonAllocator());
    if (!readJsonFile("/registry.json", registryDoc)) {
        return false;
    }
    vTaskDelay(1);

    JsonArrayConst configFiles = registryDoc[serviceUUID];
    if (configFiles.isNull()) {
        ESP_LOGE("BUTTPLUGIO", "Service UUID not found in registry.json: %s",
                 serviceUUID.c_str());
        return false;
    }

    // Search through config files to find a matching device
    for (JsonString configFileRow : configFiles) {
        const String candidate = configFileRow.c_str();

        JsonDocument configDoc(buttplugJsonAllocator());
        if (!readJsonFile(candidate, configDoc)) {
            vTaskDelay(1);
            continue;
        }
        vTaskDelay(1);

        ESP_LOGI("BUTTPLUGIO", "Opened config file: %s", candidate.c_str());

        if (!validateConfigStructure(configDoc, candidate)) {
            vTaskDelay(1);
            continue;
        }
        vTaskDelay(1);

        JsonObjectConst communication = configDoc["communication"][0];
        JsonObjectConst btleData = communication["btle"];
        JsonArrayConst names = btleData["names"];

        if (!matchesDeviceName(deviceName, names)) {
            vTaskDelay(1);
            continue;
        }
        vTaskDelay(1);

        JsonObjectConst found = extractCharacteristics(configDoc, serviceUUID);
        vTaskDelay(1);

        if (found.isNull()) {
            vTaskDelay(1);
            continue;
        }

        characteristics.set(found);
        configFileName = candidate;
        return true;
    }

    return false;
}

}  // namespace

/// @brief A device factory for devices that use the ButtplugIO protocol.
/// @param advertisedDevice The advertised BLE device to create a device for
/// @return A pointer to the created Device, or nullptr if creation failed
Device* ButtplugIODeviceFactory(
    const NimBLEAdvertisedDevice* advertisedDevice) {
    auto serviceUUID = advertisedDevice->getServiceUUID().toString();
    String deviceName = String(advertisedDevice->getName().c_str());

    String configFileName = "";
    JsonDocument characteristics;
    if (!findConfiguration(serviceUUID, deviceName, configFileName,
                           characteristics)) {
        ESP_LOGW("BUTTPLUGIO",
                 "No matching configuration found for device: %s",
                 deviceName.c_str());
        return nullptr;
    }

    // TODO: Create appropriate device type based on characteristics
    // For now, defaulting to LovenseDevice as before
    return new LovenseDevice(advertisedDevice, configFileName,
                             characteristics.as<JsonObjectConst>());
}
