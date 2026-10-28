#pragma once

// OSSM machine-geometry settings pages, opened from the control screen's
// Menu button. Edits stay local until "Restart" writes them over RAD BLE.
namespace ossmSettings {

    // Control screen -> list: forget the last read and any pending edits.
    void begin();

    // ossm_settings list. enterList() starts a read the first time; the read
    // fires a `done` event, which calls drawList() to show the values.
    void enterList();
    void drawList();
    void retryLoad();
    bool isFieldSelected();
    bool hasPending();

    // ossm_setting_edit: one number on the right encoder.
    void enterEdit();
    bool editLeavesPending();
    void keepEdit();

    // ossm_settings_confirm: Restart writes, Cancel drops pending edits.
    void enterConfirm();
    bool canRestart();
    void discardPending();

    // ossm_settings_saving: writes pending fields, then fires `done`.
    void enterSaving();
    bool saveSucceeded();

}  // namespace ossmSettings
