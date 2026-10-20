#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string>
#include <utility>
#include <vector>

#include "ossm_rad_ble_logic.h"

// RAD BLE v1 client for the OSSM. JSON requests go to the command
// characteristic; replies arrive as indications on the response
// characteristic. One request is in flight at a time and each call blocks
// for at most ossm_rad_ble::kRequestTimeoutMs, so call it from a worker task,
// never from the state machine or the BLE host task.
//
// The OSSM device owns this through a shared_ptr. Worker tasks keep their own
// copy, so a disconnect that deletes the device cannot free it mid-request.
class OssmRadBle {
  public:
    OssmRadBle();
    ~OssmRadBle();

    OssmRadBle(const OssmRadBle &) = delete;
    OssmRadBle &operator=(const OssmRadBle &) = delete;

    // Called after characteristic discovery. `response` is null on OSSM
    // firmware without RAD BLE replies.
    void attach(NimBLERemoteCharacteristic *command,
                NimBLERemoteCharacteristic *response);
    // Called when the device is deleted; later requests fail fast.
    void detach();

    bool isAvailable() const;

    // Response characteristic indication callback (BLE host task).
    void onIndication(const uint8_t *data, size_t length);

    ossm_rad_ble::Reply request(const char *op, const char *path,
                                const std::string &argsJson = "",
                                uint32_t lease = 0);

    bool readSetting(const char *path, ossm_rad_ble::SettingRead &out,
                     ossm_rad_ble::Reply &failure);

    // Acquires a control lease, writes each (path, value) in order, then
    // releases the lease straight away so the RAD App is not locked out.
    // Stops at the first failure and returns it in `failure`. Returns how
    // many writes the OSSM saved, in order.
    size_t writeSettings(
        const std::vector<std::pair<const char *, float>> &writes,
        ossm_rad_ble::Reply &failure);

  private:
    SemaphoreHandle_t requestMutex = nullptr;
    SemaphoreHandle_t replySemaphore = nullptr;
    SemaphoreHandle_t stateMutex = nullptr;  // guards pendingId/pendingReply

    NimBLERemoteCharacteristic *commandCharacteristic = nullptr;
    NimBLERemoteCharacteristic *responseCharacteristic = nullptr;
    volatile bool detached = false;

    uint32_t nextId = 0;
    uint32_t pendingId = 0;
    ossm_rad_ble::Reply pendingReply;
};
