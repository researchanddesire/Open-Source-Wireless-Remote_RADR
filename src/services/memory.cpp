#include "memory.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

AT24C04 memoryService(AT24C_ADDRESS_0);

bool isMemoryChipFound = false;

bool initMemoryService() {
    ESP_LOGI("MEMORY", "Waiting 10 seconds...");
    Wire.begin();

    uint8_t testValue = 0xA5;  // arbitrary test value
    memoryService.write(0, testValue);
    delay(5);  // short delay for eeprom write
    uint8_t verify = memoryService.read(0);
    delay(5);

    if (verify == testValue) {
        isMemoryChipFound = true;
        ESP_LOGI("MEMORY", "We found the memory!");
    } else {
        ESP_LOGI("MEMORY", "We did not find the memory!");
    }

    // regardless init littlefs
    if (!LittleFS.begin()) {
        ESP_LOGI("MEMORY", "Failed to initialize LittleFS");
    }

    // start by reading the registry.json file, if it exists
    if (LittleFS.exists("/registry.json")) {
        ESP_LOGI("MEMORY", "registry.json exists");
        File file = LittleFS.open("/registry.json", "r");
        if (file) {
            String jsonString = file.readString();
            file.close();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, jsonString);
            if (error) {
                ESP_LOGI("MEMORY", "Failed to parse registry.json: %s",
                         error.c_str());
            }
            ESP_LOGI("MEMORY", "registry.json parsed successfully");
        }
    }

    return true;
}