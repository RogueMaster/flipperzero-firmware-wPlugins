#include "zero_buster_scene.h"
#include "zero_buster.h"
#include "helpers/zero_buster_storage.h"

bool zero_buster_scene_main_on_event(ZeroBusterApp* app, InputEvent* event) {
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            if(app->active_row > 0) {
                app->active_row--;
            } else {
                app->active_row = Setting_Count - 1;
            }
            if(app->active_row == Setting_Edit_HID && app->spoof_profile != Spoof_Custom) {
                if(app->active_row > 0) {
                    app->active_row--;
                } else {
                    app->active_row = Setting_Count - 1;
                }
            }
        } else if(event->key == InputKeyDown) {
            app->active_row = (app->active_row + 1) % Setting_Count;
            if(app->active_row == Setting_Edit_HID && app->spoof_profile != Spoof_Custom) {
                app->active_row = (app->active_row + 1) % Setting_Count;
            }
        } else if(event->key == InputKeyLeft) {
            if(app->active_row == Setting_Payload && app->payloads.count > 0) {
                if(app->payloads.selected_index > 0) {
                    app->payloads.selected_index--;
                } else {
                    app->payloads.selected_index = app->payloads.count - 1;
                }
            } else if(app->active_row == Setting_Layout) {
                if(app->layout > 0) {
                    app->layout--;
                } else {
                    app->layout = Layout_Count - 1;
                }
            } else if(app->active_row == Setting_Spoof) {
                if(app->spoof_profile > 0) {
                    app->spoof_profile--;
                } else {
                    app->spoof_profile = Spoof_Count - 1;
                }
                if(app->spoof_profile == Spoof_Custom) {
                    load_custom_hid_config(app->storage, &app->custom_hid_cfg);
                }
            }
        } else if(event->key == InputKeyRight) {
            if(app->active_row == Setting_Payload && app->payloads.count > 0) {
                app->payloads.selected_index =
                    (app->payloads.selected_index + 1) % app->payloads.count;
            } else if(app->active_row == Setting_Layout) {
                app->layout = (app->layout + 1) % Layout_Count;
            } else if(app->active_row == Setting_Spoof) {
                app->spoof_profile = (app->spoof_profile + 1) % Spoof_Count;
                if(app->spoof_profile == Spoof_Custom) {
                    load_custom_hid_config(app->storage, &app->custom_hid_cfg);
                }
            }
        } else if(event->key == InputKeyOk) {
            if(app->active_row == Setting_Edit_HID) {
                app->state = State_Edit_HID;
                app->edit_cursor = 0;
            } else if(app->active_row == Setting_About) {
                app->state = State_About;
            } else {
                // Launch directly on OK from any other setting row (Payload, Layout, Spoof)
                if(app->payloads.count > 0) {
                    furi_mutex_release(app->mutex);
                    furi_thread_join(app->worker_thread);
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->worker_stop = false;
                    furi_thread_start(app->worker_thread);
                }
            }
        } else if(event->key == InputKeyBack) {
            return false; // Exit app
        }
    } else if(event->type == InputTypeLong) {
        if(event->key == InputKeyOk) {
            if(app->active_row == Setting_Spoof && app->spoof_profile == Spoof_Custom) {
                app->state = State_Edit_HID;
                app->edit_cursor = 0;
            }
        }
    }
    return true;
}
