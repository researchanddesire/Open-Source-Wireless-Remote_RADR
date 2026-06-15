#include "ossmUpdate.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <devices/device.h>
#include <pages/genericPages.h>

#include "constants/Strings.h"
#include "pages/TextPages.h"

// Forward declaration — defined in remote.cpp
extern void fireStateMachineDoneEvent();

// Flag read by guard in state machine to determine transition
bool ossmUpdateIsAvailable = false;

// OSSM Update page definitions (extern-declared in TextPages.h)
const TextPage ossmUpdateCheckPage = {
    .title = "Update OSSM",
    .description = "Checking for updates...",
    .leftButtonText = GO_BACK,
};

const TextPage ossmUpdateConfirmPage = {
    .title = "Update Available",
    .description =
        "A firmware update is available for your OSSM. This will restart "
        "the device.",
    .leftButtonText = CANCEL_STRING,
    .rightButtonText = "Update",
};

const TextPage ossmUpdateUpdatingPage = {
    .title = "Updating OSSM",
    .description =
        "Your OSSM is downloading and installing an update. "
        "Please wait...",
};

const TextPage ossmUpdateNonePage = {
    .title = "Up to Date",
    .description = "Your OSSM is running the latest firmware.",
    .leftButtonText = GO_BACK,
};

const TextPage ossmUpdateWifiPage = {
    .title = "WiFi Required",
    .description =
        "Connect to WiFi first to check for OSSM updates.",
    .leftButtonText = GO_BACK,
};

static const char *UPDATE_TAG = "OSSM_UPDATE";

// Parse a semicolon-delimited string and return the field at the given index.
// Returns empty string if the index is out of range.
static String getField(const std::string &data, int fieldIndex) {
    int currentField = 0;
    size_t fieldStart = 0;

    for (size_t i = 0; i <= data.size(); i++) {
        if (i == data.size() || data[i] == ';') {
            if (currentField == fieldIndex) {
                return String(data.substr(fieldStart, i - fieldStart).c_str());
            }
            currentField++;
            fieldStart = i + 1;
        }
    }

    return "";
}

static void ossmUpdateCheckTask(void *pvParameters) {
    ossmUpdateIsAvailable = false;

    // Guard: device still connected
    if (device == nullptr || !device->isConnected) {
        ESP_LOGW(UPDATE_TAG, "No device connected");
        updateStatusText("No device connected.");
        vTaskDelete(nullptr);
        return;
    }

    // Read pairing characteristic: "MAC;chipModel;wifiConnected;md5;version;efuseMacHex"
    auto it = device->characteristics.find("pairing");
    if (it == device->characteristics.end() ||
        it->second.pCharacteristic == nullptr) {
        ESP_LOGW(UPDATE_TAG, "Pairing characteristic not found");
        updateStatusText("Could not read device info.");
        vTaskDelete(nullptr);
        return;
    }

    NimBLERemoteCharacteristic *pairingChar = it->second.pCharacteristic;
    std::string pairingInfo;
    try {
        pairingInfo = pairingChar->readValue();
    } catch (...) {
        ESP_LOGW(UPDATE_TAG, "BLE read failed");
        updateStatusText("Could not read device info.");
        vTaskDelete(nullptr);
        return;
    }

    if (pairingInfo.empty()) {
        ESP_LOGW(UPDATE_TAG, "Empty pairing info");
        updateStatusText("Could not read device info.");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(UPDATE_TAG, "Pairing info: %s", pairingInfo.c_str());

    String version = getField(pairingInfo, 4);
    if (version.isEmpty()) {
        ESP_LOGW(UPDATE_TAG, "Could not parse version from pairing info");
        updateStatusText("Could not determine OSSM version.");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(UPDATE_TAG, "OSSM version: %s", version.c_str());

    // Check CloudFront for available update
    HTTPClient http;
    http.begin("http://d2g4f7zewm360.cloudfront.net/check-for-ossm-update");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["ossmSwVersion"] = version;
    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);
    if (httpCode != 200) {
        ESP_LOGW(UPDATE_TAG, "Update check failed with HTTP %d", httpCode);
        http.end();
        updateStatusText("Could not check for updates.");
        vTaskDelete(nullptr);
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument resp;
    DeserializationError err = deserializeJson(resp, payload);
    if (err) {
        ESP_LOGW(UPDATE_TAG, "JSON parse error: %s", err.c_str());
        updateStatusText("Invalid server response.");
        vTaskDelete(nullptr);
        return;
    }

    bool needUpdate = resp["response"]["needUpdate"].as<bool>();
    ESP_LOGI(UPDATE_TAG, "Update check result: needUpdate=%d", needUpdate);

    ossmUpdateIsAvailable = needUpdate;
    fireStateMachineDoneEvent();
    vTaskDelete(nullptr);
}

void startOssmUpdateCheck() {
    xTaskCreatePinnedToCore(ossmUpdateCheckTask, "ossmUpdateCheck",
                            20 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            1);
}
