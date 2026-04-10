#include "mfp_app.h"
#include "mfp_listener.h"
#include "mfp_dump_view.h"
#include "scenes/mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <furi.h>
#include <string.h>
#include <stdlib.h>

/* ---- Scene handlers table ---- */

void mfp_scene_start_on_enter(void*);
bool mfp_scene_start_on_event(void*, SceneManagerEvent);
void mfp_scene_start_on_exit(void*);

void mfp_scene_read_on_enter(void*);
bool mfp_scene_read_on_event(void*, SceneManagerEvent);
void mfp_scene_read_on_exit(void*);

void mfp_scene_saved_on_enter(void*);
bool mfp_scene_saved_on_event(void*, SceneManagerEvent);
void mfp_scene_saved_on_exit(void*);

void mfp_scene_dict_select_on_enter(void*);
bool mfp_scene_dict_select_on_event(void*, SceneManagerEvent);
void mfp_scene_dict_select_on_exit(void*);

void mfp_scene_read_all_on_enter(void*);
bool mfp_scene_read_all_on_event(void*, SceneManagerEvent);
void mfp_scene_read_all_on_exit(void*);

void mfp_scene_read_all_result_on_enter(void*);
bool mfp_scene_read_all_result_on_event(void*, SceneManagerEvent);
void mfp_scene_read_all_result_on_exit(void*);

void mfp_scene_actions_on_enter(void*);
bool mfp_scene_actions_on_event(void*, SceneManagerEvent);
void mfp_scene_actions_on_exit(void*);

void mfp_scene_save_name_on_enter(void*);
bool mfp_scene_save_name_on_event(void*, SceneManagerEvent);
void mfp_scene_save_name_on_exit(void*);

void mfp_scene_save_success_on_enter(void*);
bool mfp_scene_save_success_on_event(void*, SceneManagerEvent);
void mfp_scene_save_success_on_exit(void*);

void mfp_scene_delete_confirm_on_enter(void*);
bool mfp_scene_delete_confirm_on_event(void*, SceneManagerEvent);
void mfp_scene_delete_confirm_on_exit(void*);

void mfp_scene_delete_success_on_enter(void*);
bool mfp_scene_delete_success_on_event(void*, SceneManagerEvent);
void mfp_scene_delete_success_on_exit(void*);

void mfp_scene_dump_view_on_enter(void*);
bool mfp_scene_dump_view_on_event(void*, SceneManagerEvent);
void mfp_scene_dump_view_on_exit(void*);

void mfp_scene_emulate_setup_on_enter(void*);
bool mfp_scene_emulate_setup_on_event(void*, SceneManagerEvent);
void mfp_scene_emulate_setup_on_exit(void*);

void mfp_scene_emulate_on_enter(void*);
bool mfp_scene_emulate_on_event(void*, SceneManagerEvent);
void mfp_scene_emulate_on_exit(void*);

void mfp_scene_card_info_on_enter(void*);
bool mfp_scene_card_info_on_event(void*, SceneManagerEvent);
void mfp_scene_card_info_on_exit(void*);

static const SceneManagerHandlers mfp_scene_handlers = {
    .on_enter_handlers = (void(*[])(void*)){
        mfp_scene_start_on_enter,
        mfp_scene_read_on_enter,
        mfp_scene_saved_on_enter,
        mfp_scene_dict_select_on_enter,
        mfp_scene_read_all_on_enter,
        mfp_scene_read_all_result_on_enter,
        mfp_scene_actions_on_enter,
        mfp_scene_save_name_on_enter,
        mfp_scene_save_success_on_enter,
        mfp_scene_delete_confirm_on_enter,
        mfp_scene_delete_success_on_enter,
        mfp_scene_dump_view_on_enter,
        mfp_scene_emulate_setup_on_enter,
        mfp_scene_emulate_on_enter,
        mfp_scene_card_info_on_enter,
    },
    .on_event_handlers = (bool(*[])(void*, SceneManagerEvent)){
        mfp_scene_start_on_event,
        mfp_scene_read_on_event,
        mfp_scene_saved_on_event,
        mfp_scene_dict_select_on_event,
        mfp_scene_read_all_on_event,
        mfp_scene_read_all_result_on_event,
        mfp_scene_actions_on_event,
        mfp_scene_save_name_on_event,
        mfp_scene_save_success_on_event,
        mfp_scene_delete_confirm_on_event,
        mfp_scene_delete_success_on_event,
        mfp_scene_dump_view_on_event,
        mfp_scene_emulate_setup_on_event,
        mfp_scene_emulate_on_event,
        mfp_scene_card_info_on_event,
    },
    .on_exit_handlers = (void(*[])(void*)){
        mfp_scene_start_on_exit,
        mfp_scene_read_on_exit,
        mfp_scene_saved_on_exit,
        mfp_scene_dict_select_on_exit,
        mfp_scene_read_all_on_exit,
        mfp_scene_read_all_result_on_exit,
        mfp_scene_actions_on_exit,
        mfp_scene_save_name_on_exit,
        mfp_scene_save_success_on_exit,
        mfp_scene_delete_confirm_on_exit,
        mfp_scene_delete_success_on_exit,
        mfp_scene_dump_view_on_exit,
        mfp_scene_emulate_setup_on_exit,
        mfp_scene_emulate_on_exit,
        mfp_scene_card_info_on_exit,
    },
    .scene_num = MfpSceneNum,
};

/* ---- ViewDispatcher callbacks ---- */

static bool mfp_back_event_cb(void* ctx) {
    MfpApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool mfp_custom_event_cb(void* ctx, uint32_t event) {
    MfpApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/* ---- App lifecycle ---- */

static MfpApp* mfp_app_alloc(void) {
    MfpApp* app = malloc(sizeof(MfpApp));
    memset(app, 0, sizeof(*app));

    /* Defaults */
    memset(app->key, 0x00, MFP_AES_KEY_SIZE);
    app->key_type       = MfpKeyA;
    app->target_sector  = 0;
    app->allow_overwrite = true;

    /* Storage */
    app->storage = furi_record_open(RECORD_STORAGE);

    /* Ensure our data folder exists and has a human-readable README
     * + a starter dictionary file so users can start using the app
     * without manually copying anything onto the SD card. */
    storage_simply_mkdir(app->storage, MFP_APP_FOLDER);
    {
        const char* readme_path = MFP_APP_FOLDER "/README.txt";
        if(!storage_file_exists(app->storage, readme_path)) {
            File* f = storage_file_alloc(app->storage);
            if(storage_file_open(f, readme_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                const char* readme =
                    "MFP Reader — user data folder\n"
                    "\n"
                    "Place your MIFARE Plus AES key dictionaries here\n"
                    "as .dic files. They will appear in the dictionary\n"
                    "picker shown after pressing Dump.\n"
                    "\n"
                    "Format: one 32-character hex AES key per line.\n"
                    "Lines starting with # are comments.\n"
                    "\n"
                    "Example:\n"
                    "  # my test card\n"
                    "  FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n"
                    "  A0A1A2A3A4A5A6A7A8A9AAABACADAEAF\n"
                    "\n"
                    "Dumps are saved next to this file as\n"
                    "<name>.mfp (Version 2 format).\n"
                    "\n"
                    "mfp_default_keys.dic is created on first launch\n"
                    "with well-known factory / development keys. You\n"
                    "can edit it or create new .dic files alongside.\n";
                storage_file_write(f, readme, strlen(readme));
                storage_file_close(f);
            }
            storage_file_free(f);
        }

        /* Starter dictionary — never overwrite an existing one, the
         * user may have added their own keys. */
        const char* dict_path = MFP_APP_FOLDER "/mfp_default_keys.dic";
        if(!storage_file_exists(app->storage, dict_path)) {
            File* f = storage_file_alloc(app->storage);
            if(storage_file_open(f, dict_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                const char* dict =
                    "# MIFARE Plus AES default keys\n"
                    "# One 32-char hex key per line. '#' starts a comment.\n"
                    "# Edit this file or create additional .dic files in the\n"
                    "# same folder — they'll all show up under Dump -> Dict.\n"
                    "\n"
                    "# --- Factory zero / all-FF blanks ---\n"
                    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\n"
                    "00000000000000000000000000000000\n"
                    "\n"
                    "# --- NXP documented MAD / application defaults ---\n"
                    "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF\n"
                    "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF\n"
                    "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF\n"
                    "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF\n"
                    "\n"
                    "# --- NFC Forum NDEF application key ---\n"
                    "D3F7D3F7D3F7D3F7D3F7D3F7D3F7D3F7\n"
                    "\n"
                    "# --- Sequential byte patterns (common dev defaults) ---\n"
                    "00010203040506070809101112131415\n"
                    "01020304050607080910111213141516\n"
                    "0102030405060708090A0B0C0D0E0F10\n"
                    "000102030405060708090A0B0C0D0E0F\n"
                    "0F0E0D0C0B0A09080706050403020100\n"
                    "\n"
                    "# --- Repeating byte patterns ---\n"
                    "11111111111111111111111111111111\n"
                    "22222222222222222222222222222222\n"
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
                    "55555555555555555555555555555555\n"
                    "DEADBEEFDEADBEEFDEADBEEFDEADBEEF\n"
                    "CAFEBABECAFEBABECAFEBABECAFEBABE\n"
                    "\n"
                    "# --- NXP demo key from app notes ---\n"
                    "2B7E151628AED2A6ABF7158809CF4F3C\n";
                storage_file_write(f, dict, strlen(dict));
                storage_file_close(f);
            }
            storage_file_free(f);
        }
    }

    /* Notifications */
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* NFC */
    app->nfc = nfc_alloc();

    /* Dictionary */
    app->dict_path   = furi_string_alloc();
    app->dict_buf    = NULL;
    app->dict_key_count = 0;

    /* Read-all scan */
    app->scan_status_text = furi_string_alloc();
    app->read_all_mode = false;

    /* GUI */
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager   = scene_manager_alloc(&mfp_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, mfp_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, mfp_back_event_cb);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, MfpViewSubmenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, MfpViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, MfpViewWidget, widget_get_view(app->widget));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewByteInput, byte_input_get_view(app->byte_input));

    app->file_browser_result = furi_string_alloc();
    app->file_browser = file_browser_alloc(app->file_browser_result);
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewFileBrowser, file_browser_get_view(app->file_browser));

    app->emulate_view = mfp_emulate_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewEmulate, mfp_emulate_view_get_view(app->emulate_view));

    app->dump_view = mfp_dump_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewDump, mfp_dump_view_get_view(app->dump_view));

    app->result_view = mfp_result_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewResult, mfp_result_view_get_view(app->result_view));

    app->text_box = text_box_alloc();
    app->text_box_store = furi_string_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewTextBox, text_box_get_view(app->text_box));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MfpViewTextInput, text_input_get_view(app->text_input));

    return app;
}

static void mfp_app_free(MfpApp* app) {
    /* If emulator was running (e.g. forced app close), persist modifications
     * back to the dump only when allow_overwrite is set. */
    if(app->emulator) {
        MfpListener* emu = (MfpListener*)app->emulator;
        mfp_listener_stop(emu);
        if(emu->writes_count > 0 && app->allow_overwrite) {
            memcpy(app->blocks, emu->blocks, sizeof(app->blocks));
            mfp_storage_save_modifications(
                app, app->modified_save_path, sizeof(app->modified_save_path));
        }
        mfp_listener_free(emu);
        app->emulator = NULL;
    }

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
    nfc_free(app->nfc);

    if(app->dict_buf) {
        free(app->dict_buf);
    }
    furi_string_free(app->dict_path);
    furi_string_free(app->scan_status_text);

    view_dispatcher_remove_view(app->view_dispatcher, MfpViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewEmulate);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewDump);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, MfpViewTextInput);

    submenu_free(app->submenu);
    popup_free(app->popup);
    widget_free(app->widget);
    dialog_ex_free(app->dialog_ex);
    byte_input_free(app->byte_input);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_browser_result);
    mfp_emulate_view_free(app->emulate_view);
    mfp_dump_view_free(app->dump_view);
    mfp_result_view_free(app->result_view);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    text_input_free(app->text_input);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    free(app);
}

int32_t mfp_app_entry(void* p) {
    UNUSED(p);
    MfpApp* app = mfp_app_alloc();

    /* Test trigger: auto-load first .mfp dump and jump to Emulate */
    if(storage_file_exists(app->storage, "/ext/mfp_emuonly")) {
        storage_simply_remove(app->storage, "/ext/mfp_emuonly");
        app->read_all_mode = true;

        File* dir = storage_file_alloc(app->storage);
        if(storage_dir_open(dir, MFP_APP_FOLDER)) {
            FileInfo info;
            char name[64];
            while(storage_dir_read(dir, &info, name, sizeof(name))) {
                if(!(info.flags & FSF_DIRECTORY) && strstr(name, ".mfp")) {
                    char path[128];
                    snprintf(path, sizeof(path), "%s/%s", MFP_APP_FOLDER, name);
                    mfp_storage_load(app, path);
                    break;
                }
            }
            storage_dir_close(dir);
        }
        storage_file_free(dir);

        scene_manager_next_scene(app->scene_manager, MfpSceneEmulate);
    } else {
        scene_manager_next_scene(app->scene_manager, MfpSceneStart);
    }

    view_dispatcher_run(app->view_dispatcher);
    mfp_app_free(app);
    return 0;
}
