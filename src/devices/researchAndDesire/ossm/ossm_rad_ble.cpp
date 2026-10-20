#include "ossm_rad_ble.h"

static const char *RAD_TAG = "OSSM_RAD_BLE";

using ossm_rad_ble::Reply;
using ossm_rad_ble::ReplyKind;

OssmRadBle::OssmRadBle() {
    requestMutex = xSemaphoreCreateMutex();
    replySemaphore = xSemaphoreCreateBinary();
    stateMutex = xSemaphoreCreateMutex();
}

OssmRadBle::~OssmRadBle() {
    if (requestMutex != nullptr) vSemaphoreDelete(requestMutex);
    if (replySemaphore != nullptr) vSemaphoreDelete(replySemaphore);
    if (stateMutex != nullptr) vSemaphoreDelete(stateMutex);
}

void OssmRadBle::attach(NimBLERemoteCharacteristic *command,
                        NimBLERemoteCharacteristic *response) {
    commandCharacteristic = command;
    responseCharacteristic = response;
    detached = false;
}

void OssmRadBle::detach() {
    detached = true;
    // Wake a request that is waiting so it fails now instead of timing out.
    if (replySemaphore != nullptr) xSemaphoreGive(replySemaphore);
}

bool OssmRadBle::isAvailable() const {
    return !detached && commandCharacteristic != nullptr &&
           responseCharacteristic != nullptr;
}

void OssmRadBle::onIndication(const uint8_t *data, size_t length) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    const uint32_t expectedId = pendingId;
    xSemaphoreGive(stateMutex);
    if (expectedId == 0) return;

    Reply reply;
    const ReplyKind kind = ossm_rad_ble::parseReply(
        reinterpret_cast<const char *>(data), length, expectedId, reply);
    if (kind != ReplyKind::Terminal) return;

    bool matched = false;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (pendingId == expectedId) {
        pendingReply = std::move(reply);
        pendingId = 0;
        matched = true;
    }
    xSemaphoreGive(stateMutex);
    if (matched) xSemaphoreGive(replySemaphore);
}

Reply OssmRadBle::request(const char *op, const char *path,
                          const std::string &argsJson, uint32_t lease) {
    if (!isAvailable())
        return ossm_rad_ble::failedReply("unavailable",
                                         "OSSM has no RAD BLE responses");
    if (xSemaphoreTake(requestMutex,
                       pdMS_TO_TICKS(ossm_rad_ble::kRequestTimeoutMs)) !=
        pdTRUE)
        return ossm_rad_ble::failedReply("busy", "Another request is running");

    // Drop a wake-up left over from a request that already timed out.
    xSemaphoreTake(replySemaphore, 0);

    uint32_t id = ++nextId;
    if (id == 0) id = ++nextId;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    pendingId = id;
    pendingReply = Reply();
    xSemaphoreGive(stateMutex);

    const std::string message =
        ossm_rad_ble::buildRequest(id, op, path, argsJson, lease);
    ESP_LOGI(RAD_TAG, "Request %s", message.c_str());

    Reply reply;
    if (detached || !commandCharacteristic->writeValue(message)) {
        reply = ossm_rad_ble::failedReply("write_failed",
                                          "Could not send to the OSSM");
    } else if (xSemaphoreTake(replySemaphore,
                              pdMS_TO_TICKS(ossm_rad_ble::kRequestTimeoutMs)) !=
                   pdTRUE ||
               detached) {
        reply = ossm_rad_ble::failedReply("timeout", "The OSSM did not reply");
    } else {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        reply = pendingReply;
        xSemaphoreGive(stateMutex);
    }

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    pendingId = 0;
    xSemaphoreGive(stateMutex);
    xSemaphoreGive(requestMutex);

    if (!reply.ok)
        ESP_LOGW(RAD_TAG, "%s %s failed: %s %s", op, path ? path : "",
                 reply.code.c_str(), reply.message.c_str());
    return reply;
}

bool OssmRadBle::readSetting(const char *path, ossm_rad_ble::SettingRead &out,
                             Reply &failure) {
    const Reply reply = request("setting.read", path);
    if (!reply.ok) {
        failure = reply;
        return false;
    }
    if (!ossm_rad_ble::parseSettingRead(reply.resultJson, out)) {
        failure = ossm_rad_ble::failedReply("invalid_reply",
                                            "Unexpected reply from the OSSM");
        return false;
    }
    return true;
}

size_t OssmRadBle::writeSettings(
    const std::vector<std::pair<const char *, float>> &writes,
    Reply &failure) {
    const Reply acquired = request(
        "control.acquire", nullptr,
        ossm_rad_ble::ttlArgs(ossm_rad_ble::kLeaseTtlSeconds));
    uint32_t lease = 0;
    if (!acquired.ok) {
        failure = acquired;
        return 0;
    }
    if (!ossm_rad_ble::parseLease(acquired.resultJson, lease)) {
        failure = ossm_rad_ble::failedReply("invalid_reply",
                                            "The OSSM sent no control lease");
        return 0;
    }

    size_t written = 0;
    for (const auto &write : writes) {
        const Reply reply =
            request("setting.write", write.first,
                    ossm_rad_ble::valueArgs(write.second), lease);
        if (!reply.ok) {
            failure = reply;
            break;
        }
        ++written;
    }

    request("control.release", nullptr, "", lease);
    return written;
}
