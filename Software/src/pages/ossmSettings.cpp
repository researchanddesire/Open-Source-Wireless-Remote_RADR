#include "ossmSettings.h"

#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <components/TextButton.h>
#include <constants/Colors.h>
#include <constants/Sizes.h>
#include <devices/researchAndDesire/ossm/ossm_device.hpp>
#include <pins.h>
#include <services/display.h>
#include <services/encoder.h>
#include <state/remote.h>

#include <atomic>
#include <climits>
#include <memory>
#include <string>
#include <vector>

#include "controller.h"
#include "displayUtils.h"
#include "menus.h"
#include "pages/TextPages.h"
#include "utils/psramTask.h"

// Defined in remote.cpp
extern void fireStateMachineDoneEvent();

const TextPage ossmSettingsSavingPage = {
    .title = "Saving Settings",
    .description = "Writing machine settings to the OSSM...",
};

using namespace sml;
using namespace ossm_rad_ble;

namespace ossmSettings {
    namespace {

        const char *SETTINGS_TAG = "OSSM_SETTINGS";

        enum class LoadStatus { NotLoaded, Loading, Loaded, Unsupported, Failed };

        std::atomic<LoadStatus> loadStatus{LoadStatus::NotLoaded};
        std::string loadError;

        MachineConfig stored;   // saved in NVS, used after the next restart
        MachineConfig active;   // in use since the OSSM booted
        MachineConfig pending;  // local edits, starting from stored
        bool pendingValid = false;

        int editingField = 0;
        float editingOriginal = 0.0f;
        int listSelection = 0;

        std::atomic<bool> saveOk{false};
        std::string saveError;

        std::vector<MenuItem> listMenu;

        TaskHandle_t pageTaskHandle = nullptr;
        volatile bool pageTaskExitRequested = false;

        const int16_t buttonY = Display::HEIGHT - 30;

        OSSM *currentOssm() {
            if (device == nullptr || !device->hasMachineSettings())
                return nullptr;
            return static_cast<OSSM *>(device);
        }

        float shownValue(int index) {
            return pendingValid ? fieldValue(pending, index)
                                : fieldValue(active, index);
        }

        bool differsFromStored(const MachineConfig &config) {
            for (int index = 0; index < kFieldCount; ++index) {
                if (!sameValue(fieldValue(config, index),
                               fieldValue(stored, index)))
                    return true;
            }
            return false;
        }

        // ------------------------------------------------------------------
        // Page tasks (edit and confirm)
        // ------------------------------------------------------------------

        void stopPageTask() {
            if (pageTaskHandle == nullptr) return;
            pageTaskExitRequested = true;
            const TickType_t waitStart = xTaskGetTickCount();
            while (pageTaskHandle != nullptr &&
                   (xTaskGetTickCount() - waitStart) < pdMS_TO_TICKS(200)) {
                vTaskDelay(1);
            }
            if (pageTaskHandle != nullptr) {
                TaskHandle_t stuck = pageTaskHandle;
                pageTaskHandle = nullptr;
                vTaskSuspend(stuck);
                deletePsramTask(stuck);
            }
            pageTaskExitRequested = false;
        }

        void startPageTask(TaskFunction_t task, const char *name) {
            stopMenuTask();
            stopPageTask();
            createPsramTask(task, name, 6 * configMINIMAL_STACK_SIZE, nullptr,
                            5, &pageTaskHandle, 1);
        }

        void finishPageTask() {
            pageTaskHandle = nullptr;
            exitPsramTask();
        }

        void drawTitle(const char *title) {
            tft.setFont(&FreeSansBold12pt7b);
            tft.setTextColor(Colors::white);
            int16_t x1, y1;
            uint16_t w, h;
            tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
            tft.setCursor((Display::WIDTH - w) / 2,
                          Display::PageY + Display::Padding::P3 - y1);
            tft.print(title);
        }

        void drawCentered(const String &text, int16_t baseline) {
            int16_t x1, y1;
            uint16_t w, h;
            tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
            tft.setCursor((Display::WIDTH - w) / 2, baseline);
            tft.print(text);
        }

        // ------------------------------------------------------------------
        // Edit page
        // ------------------------------------------------------------------

        const int16_t editValueY = Display::PageY + 60;
        const int16_t editAreaHeight = buttonY - editValueY - 4;

        void drawEditValue(int index, float value) {
            MachineConfig candidate = pendingValid ? pending : stored;
            *field(candidate, index) = value;
            const GeometryError error = validateGeometry(candidate);

            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) != pdTRUE)
                return;
            tft.fillRect(0, editValueY, Display::WIDTH, editAreaHeight,
                         Colors::black);

            tft.setFont(&FreeSansBold12pt7b);
            tft.setTextColor(Colors::white);
            drawCentered(formatValue(index, value).c_str(), editValueY + 30);

            tft.setFont(&FreeSans9pt7b);
            tft.setTextColor(Colors::lightGray);
            drawCentered(("Was " + formatValue(index, editingOriginal)).c_str(),
                         editValueY + 60);

            if (error != GeometryError::None) {
                tft.setFont();
                tft.setTextColor(Colors::red);
                wrapText(tft, geometryErrorMessage(error),
                         {.x = Display::Padding::P2,
                          .y = editValueY + 80,
                          .rightPadding = Display::Padding::P2});
            }
            xSemaphoreGive(displayMutex);
        }

        void editTask(void *) {
            const int index = editingField;
            const FieldSpec &spec = kFields[index];

            rightEncoder.setBoundaries(spec.encoderMin, spec.encoderMax, false);
            rightEncoder.setAcceleration(
                spec.encoderMax - spec.encoderMin > 1000 ? 250 : 50);
            rightEncoder.setEncoderValue(toEncoder(index, editingOriginal));

            clearPage();
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                drawTitle(spec.label);
                xSemaphoreGive(displayMutex);
            }
            TextButton cancelButton(CANCEL_STRING, pins::BTN_UNDER_L, 20,
                                    buttonY);
            cancelButton.tick();
            TextButton keepButton("Keep", pins::BTN_UNDER_R,
                                  Display::WIDTH - 90, buttonY);
            keepButton.tick();

            long lastCounts = LONG_MIN;
            while (stateMachine->is("ossm_setting_edit"_s) &&
                   !pageTaskExitRequested) {
                const long counts = rightEncoder.readEncoder();
                if (counts != lastCounts) {
                    lastCounts = counts;
                    drawEditValue(index, fromEncoder(index, counts));
                }
                vTaskDelay(16 / portTICK_PERIOD_MS);
            }
            finishPageTask();
        }

        // ------------------------------------------------------------------
        // Confirm page
        // ------------------------------------------------------------------

        struct ConfirmStatus {
            bool canRestart;
            String text;
        };

        ConfirmStatus confirmStatus() {
            if (!pendingValid || !differsFromStored(pending))
                return {false, "No changes to save."};
            const GeometryError error = validateGeometry(pending);
            if (error != GeometryError::None)
                return {false, geometryErrorMessage(error)};
            std::array<int, kFieldCount> order{};
            if (planWrites(stored, pending, order) < 0)
                return {false, "Change fewer settings at a time."};
            if (device == nullptr || !device->canOpenSettings())
                return {false, "Turn the speed to 0 to restart."};
            if (!saveError.empty()) return {true, saveError.c_str()};
            return {true,
                    "Geometry changes apply only after the OSSM restarts."};
        }

        const int16_t confirmStatusY = Display::PageY + 152;

        void drawConfirmStatus(const ConfirmStatus &status) {
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) != pdTRUE)
                return;
            tft.fillRect(0, confirmStatusY, Display::WIDTH,
                         buttonY - confirmStatusY - 2, Colors::black);
            tft.setFont();
            const bool isProblem = !status.canRestart || !saveError.empty();
            tft.setTextColor(isProblem ? Colors::red : Colors::lightGray);
            wrapText(tft, status.text,
                     {.x = Display::Padding::P2,
                      .y = confirmStatusY,
                      .rightPadding = Display::Padding::P2});
            xSemaphoreGive(displayMutex);
        }

        void confirmTask(void *) {
            clearPage();
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                drawTitle("Restart OSSM?");
                tft.setFont(&FreeSans9pt7b);
                tft.setTextColor(Colors::textForeground);
                int16_t rowY = Display::PageY + 62;
                for (int index = 0; index < kFieldCount; ++index) {
                    const float from = fieldValue(stored, index);
                    const float to = fieldValue(pending, index);
                    if (!pendingValid || sameValue(from, to)) continue;
                    tft.setCursor(Display::Padding::P2, rowY);
                    tft.print((std::string(kFields[index].label) + ": " +
                               formatValue(index, from) + " > " +
                               formatValue(index, to))
                                  .c_str());
                    rowY += 18;
                }
                xSemaphoreGive(displayMutex);
            }

            TextButton cancelButton(CANCEL_STRING, pins::BTN_UNDER_L, 20,
                                    buttonY);
            cancelButton.tick();
            TextButton editButton("Edit", pins::BTN_UNDER_C,
                                  (Display::WIDTH - 70) / 2, buttonY);
            editButton.tick();
            TextButton restartButton("Restart", pins::BTN_UNDER_R,
                                     Display::WIDTH - 90, buttonY);

            bool first = true;
            ConfirmStatus last{false, ""};
            while (stateMachine->is("ossm_settings_confirm"_s) &&
                   !pageTaskExitRequested) {
                const ConfirmStatus status = confirmStatus();
                if (first || status.canRestart != last.canRestart ||
                    status.text != last.text) {
                    first = false;
                    last = status;
                    restartButton.setColors(status.canRestart
                                                ? Colors::textBackground
                                                : Colors::disabled,
                                            Colors::black);
                    drawConfirmStatus(status);
                }
                restartButton.tick();
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            finishPageTask();
        }

        // ------------------------------------------------------------------
        // BLE worker tasks
        // ------------------------------------------------------------------

        std::shared_ptr<OssmRadBle> *takeClient() {
            OSSM *ossm = currentOssm();
            if (ossm == nullptr) return nullptr;
            return new std::shared_ptr<OssmRadBle>(ossm->radBle);
        }

        void loadTask(void *param) {
            auto *holder = static_cast<std::shared_ptr<OssmRadBle> *>(param);
            std::shared_ptr<OssmRadBle> client = *holder;
            delete holder;

            MachineConfig newStored;
            MachineConfig newActive;
            LoadStatus result = LoadStatus::Loaded;
            std::string error;

            if (!client->isAvailable()) {
                result = LoadStatus::Unsupported;
            } else {
                for (int index = 0; index < kFieldCount; ++index) {
                    SettingRead read;
                    Reply failure;
                    if (!client->readSetting(kFields[index].path, read,
                                             failure)) {
                        result = meansFirmwareTooOld(failure)
                                     ? LoadStatus::Unsupported
                                     : LoadStatus::Failed;
                        error = failure.message.empty() ? failure.code
                                                        : failure.message;
                        break;
                    }
                    *field(newStored, index) = read.value;
                    *field(newActive, index) = read.active;
                }
            }

            if (result == LoadStatus::Loaded) {
                stored = newStored;
                active = newActive;
            }
            loadError = error;
            ESP_LOGI(SETTINGS_TAG, "Load finished: %d %s",
                     static_cast<int>(result), error.c_str());
            loadStatus = result;
            fireStateMachineDoneEvent();
            vTaskDelete(nullptr);
        }

        void startLoad() {
            auto *holder = takeClient();
            if (holder == nullptr) {
                loadStatus = LoadStatus::Unsupported;
                return;
            }
            loadStatus = LoadStatus::Loading;
            if (xTaskCreatePinnedToCore(loadTask, "ossmSettingsLoad",
                                        8 * configMINIMAL_STACK_SIZE, holder,
                                        1, nullptr, 1) != pdPASS) {
                delete holder;
                loadError = "Could not start the read";
                loadStatus = LoadStatus::Failed;
            }
        }

        void saveTask(void *param) {
            auto *holder = static_cast<std::shared_ptr<OssmRadBle> *>(param);
            std::shared_ptr<OssmRadBle> client = *holder;
            delete holder;

            std::array<int, kFieldCount> order{};
            const int count = planWrites(stored, pending, order);
            std::vector<std::pair<const char *, float>> writes;
            for (int step = 0; step < count; ++step) {
                writes.emplace_back(kFields[order[step]].path,
                                    fieldValue(pending, order[step]));
            }

            Reply failure;
            const size_t written =
                count > 0 ? client->writeSettings(writes, failure) : 0;
            // Keep the cache in step with what the OSSM saved.
            for (size_t step = 0; step < written; ++step) {
                *field(stored, order[step]) = fieldValue(pending, order[step]);
            }

            if (count > 0 && written == writes.size()) {
                saveError.clear();
                pendingValid = false;
                loadStatus = LoadStatus::NotLoaded;
                saveOk = true;
            } else {
                if (count <= 0) {
                    saveError = "Change fewer settings at a time.";
                } else if (!failure.message.empty()) {
                    saveError = failure.message;
                } else {
                    saveError = "OSSM error: " + failure.code;
                }
                saveOk = false;
            }
            fireStateMachineDoneEvent();
            vTaskDelete(nullptr);
        }

        // ------------------------------------------------------------------
        // List
        // ------------------------------------------------------------------

        std::optional<std::string> fieldDescription(int index) {
            const float storedValue = fieldValue(stored, index);
            if (pendingValid &&
                !sameValue(fieldValue(pending, index), storedValue)) {
                return "Was " + formatValue(index, storedValue) +
                       ". Restart the OSSM to apply.";
            }
            const float activeValue = fieldValue(active, index);
            if (!sameValue(storedValue, activeValue)) {
                return "In use: " + formatValue(index, activeValue) +
                       ". Saved: " + formatValue(index, storedValue) +
                       ", used after a restart.";
            }
            return std::nullopt;
        }

        void addInfoItem(const char *name, const uint8_t *bitmap,
                         std::optional<std::string> description) {
            MenuItem item{MenuItemE::OSSM_SETTINGS_INFO, name, bitmap,
                          std::move(description)};
            item.color = Colors::textForegroundSecondary;
            item.unfocusedColor = Colors::disabled;
            listMenu.push_back(item);
        }

        void buildListMenu() {
            listMenu.clear();
            listMenu.push_back(
                {MenuItemE::OSSM_SETTINGS_MAIN_MENU, "Main Menu", bitmap_back});

            switch (loadStatus.load()) {
                case LoadStatus::Loaded:
                    for (int index = 0; index < kFieldCount; ++index) {
                        MenuItem item{MenuItemE::OSSM_SETTING_FIELD,
                                      formatField(index, shownValue(index)),
                                      bitmap_settings, fieldDescription(index)};
                        item.metaIndex = index;
                        listMenu.push_back(item);
                    }
                    break;
                case LoadStatus::Unsupported:
                    addInfoItem("Update OSSM to change settings", bitmap_update,
                                "This OSSM firmware cannot change machine "
                                "settings from the remote.");
                    break;
                case LoadStatus::Failed:
                    addInfoItem("Could not read settings", bitmap_restart,
                                loadError + ". Select to try again.");
                    break;
                case LoadStatus::NotLoaded:
                case LoadStatus::Loading:
                    addInfoItem("Loading...", bitmap_update, std::nullopt);
                    break;
            }
        }

    }  // namespace

    void begin() {
        if (loadStatus != LoadStatus::Loading) {
            loadStatus = LoadStatus::NotLoaded;
        }
        pendingValid = false;
        saveError.clear();
        saveOk = false;
        listSelection = 0;
    }

    void enterList() {
        if (loadStatus == LoadStatus::NotLoaded) startLoad();
        drawList();
    }

    void drawList() {
        stopPageTask();
        stopMenuTask();
        stopControllerTask();
        // The menu task adds a fresh speed readout each time it starts.
        if (device != nullptr) device->clearDisplayObjects();

        buildListMenu();
        tabBarHeight = 0;
        activeMenu = &listMenu;
        activeMenuCount = listMenu.size();
        currentOption = constrain(listSelection, 0, activeMenuCount - 1);
        drawMenu();
    }

    void retryLoad() {
        if (loadStatus != LoadStatus::Failed) return;
        listSelection = currentOption;
        startLoad();
        drawList();
    }

    bool isFieldSelected() {
        if (loadStatus != LoadStatus::Loaded || activeMenu != &listMenu)
            return false;
        const int index = rightEncoder.readEncoder();
        return index >= 0 && index < static_cast<int>(listMenu.size()) &&
               listMenu[index].id == MenuItemE::OSSM_SETTING_FIELD;
    }

    bool hasPending() { return pendingValid && differsFromStored(pending); }

    void enterEdit() {
        listSelection = rightEncoder.readEncoder();
        editingField = constrain(listMenu[listSelection].metaIndex, 0,
                                 kFieldCount - 1);
        editingOriginal = shownValue(editingField);
        startPageTask(editTask, "ossmSettingEdit");
    }

    bool editLeavesPending() {
        MachineConfig candidate = pendingValid ? pending : stored;
        *field(candidate, editingField) =
            fromEncoder(editingField, rightEncoder.readEncoder());
        return differsFromStored(candidate);
    }

    void keepEdit() {
        if (!pendingValid) {
            pending = stored;
            pendingValid = true;
        }
        *field(pending, editingField) =
            fromEncoder(editingField, rightEncoder.readEncoder());
        saveError.clear();
        if (!differsFromStored(pending)) pendingValid = false;
    }

    void enterConfirm() {
        if (activeMenu == &listMenu) {
            listSelection = constrain(currentOption, 0,
                                      static_cast<int>(listMenu.size()) - 1);
        }
        startPageTask(confirmTask, "ossmSettingsConfirm");
    }

    bool canRestart() { return confirmStatus().canRestart; }

    void discardPending() {
        pendingValid = false;
        saveError.clear();
    }

    void enterSaving() {
        stopPageTask();
        saveOk = false;
        auto *holder = takeClient();
        if (holder == nullptr ||
            xTaskCreatePinnedToCore(saveTask, "ossmSettingsSave",
                                    8 * configMINIMAL_STACK_SIZE, holder, 1,
                                    nullptr, 1) != pdPASS) {
            delete holder;
            saveError = "Could not start saving";
            // Leave the state machine's on_entry before sending `done`.
            xTimerPendFunctionCall(
                [](void *, uint32_t) { fireStateMachineDoneEvent(); }, nullptr,
                0, pdMS_TO_TICKS(10));
        }
    }

    bool saveSucceeded() { return saveOk; }

}  // namespace ossmSettings
