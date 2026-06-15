#include "update.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include "state/remote.h"

static const char *UPDATE_TAG = "UPDATE";

TaskHandle_t updateTaskHandle = NULL;
TaskHandle_t updateFilesystemTaskHandle = NULL;
TaskHandle_t updateSoftwareTaskHandle = NULL;

#ifdef FORCE_UPDATE
bool isSoftwareUpdateAvailable = true;
bool isFilesystemUpdateAvailable = true;
#else
bool isSoftwareUpdateAvailable = false;
bool isFilesystemUpdateAvailable = false;
#endif

bool isUpdateAvailable() {
    if (WiFiClass::status() != WL_CONNECTED) {
        return false;
    }

#ifdef FORCE_UPDATE
    return true;
#endif

#ifdef NO_UPDATE
    return false;
#endif

    // TODO: check for updates
    return isSoftwareUpdateAvailable || isFilesystemUpdateAvailable;
}

void updateTask(void *pvParameters) {
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (isUpdateAvailable()) {
            break;
        }
    }

    stateMachine->process_event(done{});
    vTaskDelete(NULL);
}

void updateFilesystemTask(void *pvParameters) {
    ESP_LOGI(UPDATE_TAG, "Starting filesystem OTA update");

    // Check WiFi connection
    if (WiFiClass::status() != WL_CONNECTED) {
        ESP_LOGE(UPDATE_TAG, "WiFi not connected, cannot update filesystem");
        vTaskDelete(NULL);
        return;
    }

    // Unmount LittleFS before updating the partition
    LittleFS.end();
    ESP_LOGI(UPDATE_TAG, "LittleFS unmounted for OTA update");

    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate verification for Supabase

    // Construct the URL for littlefs.bin
    String url = String(UPDATE_SERVER_URL) + "/master/littlefs.bin";
    ESP_LOGI(UPDATE_TAG, "Downloading filesystem from: %s", url.c_str());

    // Configure HTTPUpdate for filesystem update
    httpUpdate.rebootOnUpdate(false);  // Don't reboot after filesystem update

    t_httpUpdate_return ret = httpUpdate.updateSpiffs(client, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            ESP_LOGE(UPDATE_TAG, "Filesystem update failed. Error (%d): %s",
                     httpUpdate.getLastError(),
                     httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            ESP_LOGI(UPDATE_TAG, "No filesystem updates available");
            break;

        case HTTP_UPDATE_OK:
            ESP_LOGI(UPDATE_TAG, "Filesystem update successful");
            break;
    }

    client.stop();

    // Remount LittleFS after update
    if (!LittleFS.begin()) {
        ESP_LOGE(UPDATE_TAG, "Failed to remount LittleFS after update");
    } else {
        ESP_LOGI(UPDATE_TAG, "LittleFS remounted successfully");
    }

    isFilesystemUpdateAvailable = false;
    vTaskDelete(NULL);
}

void updateSoftwareTask(void *pvParameters) {
    ESP_LOGI(UPDATE_TAG, "Starting firmware OTA update");

    // Check WiFi connection
    if (WiFiClass::status() != WL_CONNECTED) {
        ESP_LOGE(UPDATE_TAG, "WiFi not connected, cannot update firmware");
        vTaskDelete(NULL);
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate verification for Supabase

    // Construct the URL for firmware.bin
    String url = String(UPDATE_SERVER_URL) + "/master/firmware.bin";
    ESP_LOGI(UPDATE_TAG, "Downloading firmware from: %s", url.c_str());

    // Configure HTTPUpdate for firmware update
    httpUpdate.rebootOnUpdate(true);  // Reboot after successful firmware update

    t_httpUpdate_return ret = httpUpdate.update(client, url);

    // If we get here, the update failed (successful update triggers reboot)
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            ESP_LOGE(UPDATE_TAG, "Firmware update failed. Error (%d): %s",
                     httpUpdate.getLastError(),
                     httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            ESP_LOGI(UPDATE_TAG, "No firmware updates available");
            break;

        case HTTP_UPDATE_OK:
            // Should not reach here as rebootOnUpdate is true
            ESP_LOGI(UPDATE_TAG, "Firmware update successful, rebooting...");
            esp_restart();
            break;
    }

    client.stop();
    isSoftwareUpdateAvailable = false;
    vTaskDelete(NULL);
}