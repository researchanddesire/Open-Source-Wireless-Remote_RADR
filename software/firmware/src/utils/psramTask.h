#ifndef RADR_PSRAM_TASK_H
#define RADR_PSRAM_TASK_H
#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Creates a task whose stack lives in PSRAM instead of internal RAM, falling
// back to a normal internal-RAM task when PSRAM is unavailable.
//
// Internal RAM on RADR is nearly exhausted by the BLE and Wi-Fi stacks, and
// every FreeRTOS task stack normally comes out of it. Tasks that only draw the
// display, drive LEDs, or poll inputs can safely run from a PSRAM stack.
//
// Fallback: the production build targets the N16R8 module (octal PSRAM). On an
// N16R2 module (quad PSRAM) that build cannot initialise PSRAM, so psramFound()
// is false and the task is created on an internal stack exactly as before.
//
// Rules for tasks created with this helper:
//   * The task must never perform flash operations itself (NVS, Preferences,
//     LittleFS writes, OTA). The cache is disabled during flash writes and a
//     PSRAM stack becomes unreachable.
//   * Delete with deletePsramTask() (from another task) or exitPsramTask()
//     (from itself), never plain vTaskDelete(). Both pick the correct delete
//     call for how the task was actually created.
inline BaseType_t createPsramTask(TaskFunction_t task, const char *name,
                                  uint32_t stackBytes, void *param,
                                  UBaseType_t priority, TaskHandle_t *handle,
                                  BaseType_t core) {
    BaseType_t result = pdFAIL;
    if (psramFound()) {
        result = xTaskCreatePinnedToCoreWithCaps(
            task, name, stackBytes, param, priority, handle, core,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (result != pdPASS) {
        result = xTaskCreatePinnedToCore(task, name, stackBytes, param,
                                         priority, handle, core);
    }
    if (result != pdPASS) {
        ESP_LOGE("TASK", "Could not create task %s (%u byte stack)", name,
                 (unsigned)stackBytes);
        if (handle != nullptr) *handle = nullptr;
    }
    return result;
}

// Internal-RAM task creation that never fails silently: logs the stack size
// and the internal heap state when the task cannot be created, and nulls the
// handle. Use this instead of a bare xTaskCreatePinnedToCore for every task
// whose stack must stay in internal RAM (anything touching flash or TLS).
inline BaseType_t createInternalTask(TaskFunction_t task, const char *name,
                                     uint32_t stackBytes, void *param,
                                     UBaseType_t priority, TaskHandle_t *handle,
                                     BaseType_t core) {
    const BaseType_t result = xTaskCreatePinnedToCore(task, name, stackBytes,
                                                      param, priority, handle,
                                                      core);
    if (result != pdPASS) {
        ESP_LOGE("TASK",
                 "Could not create task %s (%u byte stack): internal free=%u "
                 "largest=%u",
                 name, (unsigned)stackBytes,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                   MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(
                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (handle != nullptr) *handle = nullptr;
    }
    return result;
}

// createInternalTask() for a task that must not fail because internal RAM is
// briefly busy. Internal RAM is nearly full once Wi-Fi and BLE run, and other
// short-lived users (a scan monitor stack, a TLS handshake) can hold the only
// free block large enough for this stack for a second or two. Waits up to
// waitMs for a large enough block before creating the task; blocks the caller
// meanwhile.
inline BaseType_t createInternalTaskWithWait(TaskFunction_t task,
                                             const char *name,
                                             uint32_t stackBytes, void *param,
                                             UBaseType_t priority,
                                             TaskHandle_t *handle,
                                             BaseType_t core,
                                             uint32_t waitMs) {
    // The stack is one block; allow for the heap's own block header.
    const size_t needed = stackBytes + 64;
    const TickType_t start = xTaskGetTickCount();
    bool waited = false;
    while (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                            MALLOC_CAP_8BIT) < needed &&
           (xTaskGetTickCount() - start) < pdMS_TO_TICKS(waitMs)) {
        if (!waited) {
            ESP_LOGW("TASK",
                     "Waiting for internal RAM to create task %s (%u byte "
                     "stack): largest=%u",
                     name, (unsigned)stackBytes,
                     (unsigned)heap_caps_get_largest_free_block(
                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            waited = true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (waited) {
        ESP_LOGW("TASK", "Waited %u ms for internal RAM for task %s",
                 (unsigned)pdTICKS_TO_MS(xTaskGetTickCount() - start), name);
    }
    return createInternalTask(task, name, stackBytes, param, priority, handle,
                              core);
}

// True when the task's stack was allocated by xTaskCreate*WithCaps (which
// creates a statically-allocated task around caller-provided buffers).
inline bool isPsramTask(TaskHandle_t handle) {
    StackType_t *stack = nullptr;
    StaticTask_t *tcb = nullptr;
    return xTaskGetStaticBuffers(handle, &stack, &tcb) == pdTRUE;
}

inline void deletePsramTask(TaskHandle_t handle) {
    if (handle == nullptr) return;
    if (isPsramTask(handle)) {
        vTaskDeleteWithCaps(handle);
    } else {
        vTaskDelete(handle);
    }
}

[[noreturn]] inline void exitPsramTask() {
    if (isPsramTask(xTaskGetCurrentTaskHandle())) {
        vTaskDeleteWithCaps(NULL);
    } else {
        vTaskDelete(NULL);
    }
    // Neither delete returns for the calling task; satisfy [[noreturn]].
    for (;;) vTaskDelay(portMAX_DELAY);
}

#endif  // RADR_PSRAM_TASK_H
