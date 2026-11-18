#include "zero_buster_scene.h"
#include "zero_buster.h"
#include "helpers/zero_buster_hid.h"
#include "helpers/zero_buster_storage.h"

bool zero_buster_scene_edit_hid_on_event(ZeroBusterApp* app, InputEvent* event) {
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            if(app->edit_cursor > 0) {
                app->edit_cursor--;
            } else {
                app->edit_cursor = 10;
            }
        } else if(event->key == InputKeyRight) {
            app->edit_cursor = (app->edit_cursor + 1) % 11;
        } else if(event->key == InputKeyUp) {
            if(app->edit_cursor < 4) {
                uint8_t n = (get_hex_nibble(app->custom_hid_cfg.vid, app->edit_cursor) + 1) % 16;
                app->custom_hid_cfg.vid =
                    set_hex_nibble(app->custom_hid_cfg.vid, app->edit_cursor, n);
            } else if(app->edit_cursor < 8) {
                uint8_t n =
                    (get_hex_nibble(app->custom_hid_cfg.pid, app->edit_cursor - 4) + 1) % 16;
                app->custom_hid_cfg.pid =
                    set_hex_nibble(app->custom_hid_cfg.pid, app->edit_cursor - 4, n);
            } else if(app->edit_cursor == 8) {
                app->mfg_preset_idx = (app->mfg_preset_idx + 1) % CUSTOM_MFG_COUNT;
                strncpy(
                    app->custom_hid_cfg.manuf,
                    custom_mfg_presets[app->mfg_preset_idx],
                    sizeof(app->custom_hid_cfg.manuf) - 1);
            } else if(app->edit_cursor == 9) {
                app->prod_preset_idx = (app->prod_preset_idx + 1) % CUSTOM_PROD_COUNT;
                strncpy(
                    app->custom_hid_cfg.product,
                    custom_prod_presets[app->prod_preset_idx],
                    sizeof(app->custom_hid_cfg.product) - 1);
            } else if(app->edit_cursor == 10) {
                app->edit_cursor = 9;
            }
        } else if(event->key == InputKeyDown) {
            if(app->edit_cursor < 4) {
                uint8_t n = get_hex_nibble(app->custom_hid_cfg.vid, app->edit_cursor);
                n = (n > 0) ? (n - 1) : 15;
                app->custom_hid_cfg.vid =
                    set_hex_nibble(app->custom_hid_cfg.vid, app->edit_cursor, n);
            } else if(app->edit_cursor < 8) {
                uint8_t n = get_hex_nibble(app->custom_hid_cfg.pid, app->edit_cursor - 4);
                n = (n > 0) ? (n - 1) : 15;
                app->custom_hid_cfg.pid =
                    set_hex_nibble(app->custom_hid_cfg.pid, app->edit_cursor - 4, n);
            } else if(app->edit_cursor == 8) {
                app->mfg_preset_idx = (app->mfg_preset_idx > 0) ? (app->mfg_preset_idx - 1) :
                                                                  (CUSTOM_MFG_COUNT - 1);
                strncpy(
                    app->custom_hid_cfg.manuf,
                    custom_mfg_presets[app->mfg_preset_idx],
                    sizeof(app->custom_hid_cfg.manuf) - 1);
            } else if(app->edit_cursor == 9) {
                app->prod_preset_idx = (app->prod_preset_idx > 0) ? (app->prod_preset_idx - 1) :
                                                                    (CUSTOM_PROD_COUNT - 1);
                strncpy(
                    app->custom_hid_cfg.product,
                    custom_prod_presets[app->prod_preset_idx],
                    sizeof(app->custom_hid_cfg.product) - 1);
            } else if(app->edit_cursor == 10) {
                app->edit_cursor = 0;
            }
        } else if(event->key == InputKeyOk) {
            if(app->edit_cursor == 10) {
                save_custom_hid_config(app->storage, &app->custom_hid_cfg);
                app->state = State_Idle;
            } else {
                app->edit_cursor = (app->edit_cursor + 1) % 11;
            }
        } else if(event->key == InputKeyBack) {
            save_custom_hid_config(app->storage, &app->custom_hid_cfg);
            app->state = State_Idle;
        }
    }
    return true;
}
