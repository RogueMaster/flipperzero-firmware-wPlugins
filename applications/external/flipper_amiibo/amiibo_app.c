#include "amiibo_crypto.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>
#include <toolbox/path.h>

typedef enum {
    ViewMenu,
    ViewTag,
    ViewEmulate,
} ViewId;

typedef enum {
    MenuBrowse = 1,
    MenuEmulate,
    MenuAuto,
    MenuSave,
    MenuKeys,
} MenuId;

typedef enum {
    UidFollowSetting,
    UidForceOn,
    UidForceOff,
} UidPolicy;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* submenu;
    Widget* widget;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Storage* storage;
    FuriString* path;
    FuriString* info;

    NfcDevice* device;
    Nfc* nfc;
    NfcListener* listener;

    uint8_t tag[AMIIBO_TAG_SIZE];
    uint8_t bin_suffix[32];
    size_t bin_size;
    AmiiboKeys keys;
    bool loaded;
    bool dirty;
    bool auto_uid;
    bool keys_loaded;
    bool emulating;
    UidPolicy policy;
    ViewId current;
    char key_path[96];
    uint32_t menu_index;
} App;

static void show_menu(App* app);
static void show_tag(App* app);
static void widget_callback(GuiButtonType button, InputType type, void* context);
static void submenu_callback(void* context, uint32_t index);

static bool ends_with_i(const char* text, const char* suffix) {
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if(text_len < suffix_len) return false;
    for(size_t i = 0; i < suffix_len; i++) {
        char left = text[text_len - suffix_len + i];
        char right = suffix[i];
        if(left >= 'A' && left <= 'Z') left = (char)(left - 'A' + 'a');
        if(right >= 'A' && right <= 'Z') right = (char)(right - 'A' + 'a');
        if(left != right) return false;
    }
    return true;
}

static bool path_has_dir(const char* path, const char* dir) {
    const char* cursor = path;
    size_t dir_len = strlen(dir);
    while(*cursor) {
        if(*cursor == '/' || *cursor == '\\') cursor++;
        const char* slash = cursor;
        while(*slash && *slash != '/' && *slash != '\\')
            slash++;
        size_t part_len = (size_t)(slash - cursor);
        if(part_len == dir_len) {
            bool match = true;
            for(size_t i = 0; i < dir_len; i++) {
                char left = cursor[i];
                char right = dir[i];
                if(left >= 'A' && left <= 'Z') left = (char)(left - 'A' + 'a');
                if(right >= 'A' && right <= 'Z') right = (char)(right - 'A' + 'a');
                if(left != right) {
                    match = false;
                    break;
                }
            }
            if(match) return true;
        }
        cursor = slash;
    }
    return false;
}

static void app_alert(App* app, const char* header, const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

static void format_uid(const uint8_t uid[7], char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%02X%02X%02X%02X%02X%02X%02X",
        uid[0],
        uid[1],
        uid[2],
        uid[3],
        uid[4],
        uid[5],
        uid[6]);
}

static void make_uid(uint8_t uid[7]) {
    furi_hal_random_fill_buf(uid, 7);
    uid[0] = 0x04;
}

static bool auto_uid_enabled(const App* app) {
    if(app->policy == UidForceOn) return true;
    if(app->policy == UidForceOff) return false;
    return app->auto_uid;
}

static const char* policy_label(const App* app) {
    if(app->policy == UidForceOn) return "ZE: new ID each scan";
    if(app->policy == UidForceOff) return "SSB: file ID kept";
    return app->auto_uid ? "Auto UID: on" : "Auto UID: off";
}

static void load_settings(App* app) {
    app->auto_uid = true;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, APP_DATA_PATH("auto_uid"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char value = '1';
        if(storage_file_read(file, &value, 1) == 1) app->auto_uid = value != '0';
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void save_settings(App* app) {
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, APP_DATA_PATH("auto_uid"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char value = app->auto_uid ? '1' : '0';
        storage_file_write(file, &value, 1);
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void load_keys(App* app) {
    const char* paths[] = {
        APP_DATA_PATH("key_retail.bin"),
        "/ext/nfc/assets/amiibo_keys.bin",
        "/ext/nfc/amiibo_keys.bin",
    };
    app->keys_loaded = false;
    app->key_path[0] = '\0';
    uint8_t bytes[AMIIBO_KEY_SIZE];
    for(size_t i = 0; i < COUNT_OF(paths); i++) {
        File* file = storage_file_alloc(app->storage);
        bool opened = storage_file_open(file, paths[i], FSAM_READ, FSOM_OPEN_EXISTING);
        size_t read = 0;
        if(opened && storage_file_size(file) == sizeof(bytes)) {
            read = storage_file_read(file, bytes, sizeof(bytes));
        }
        storage_file_close(file);
        storage_file_free(file);
        if(read == sizeof(bytes) && amiibo_keys_from_bytes(&app->keys, bytes, read)) {
            app->keys_loaded = true;
            snprintf(app->key_path, sizeof(app->key_path), "%s", paths[i]);
            return;
        }
    }
}

static void pages_from_tag(App* app) {
    MfUltralightData* data = mf_ultralight_alloc();
    if(nfc_device_get_protocol(app->device) == NfcProtocolMfUltralight) {
        mf_ultralight_copy(data, nfc_device_get_data(app->device, NfcProtocolMfUltralight));
    } else {
        Iso14443_3aData* iso = data->iso14443_3a_data;
        memset(data, 0, sizeof(MfUltralightData));
        data->iso14443_3a_data = iso;
        iso14443_3a_reset(iso);
        iso->atqa[0] = 0x44;
        iso->atqa[1] = 0x00;
        iso->sak = 0x00;

        data->type = MfUltralightTypeNTAG215;
        data->pages_total = 135;
        data->pages_read = 135;
        data->version.header = 0x00;
        data->version.vendor_id = 0x04;
        data->version.prod_type = 0x04;
        data->version.prod_subtype = 0x02;
        data->version.prod_ver_major = 0x01;
        data->version.prod_ver_minor = 0x00;
        data->version.storage_size = 0x11;
        data->version.protocol_type = 0x03;
        if(app->bin_size == 572) {
            memcpy(data->signature.data, app->bin_suffix, sizeof(app->bin_suffix));
        }
        for(size_t i = 0; i < 3; i++) {
            data->tearing_flag[i].data = MF_ULTRALIGHT_TEARING_FLAG_DEFAULT;
        }
    }
    for(size_t page = 0; page < 135; page++) {
        memcpy(data->page[page].data, app->tag + (page * 4), 4);
    }

    uint8_t uid[7];
    amiibo_uid_from_tag(app->tag, uid);
    mf_ultralight_set_uid(data, uid, sizeof(uid));
    nfc_device_set_data(app->device, NfcProtocolMfUltralight, data);
    mf_ultralight_free(data);
}

static void tag_from_pages(App* app, const MfUltralightData* data) {
    uint16_t count = data->pages_read;
    if(count > 135) count = 135;
    for(uint16_t page = 0; page < count; page++) {
        memcpy(app->tag + (page * 4), data->page[page].data, 4);
    }
}

static bool read_bin(App* app, const char* path) {
    uint8_t buffer[572];
    File* file = storage_file_alloc(app->storage);
    size_t read = 0;
    uint64_t size = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size = storage_file_size(file);
        if(size == 520 || size == 540 || size == 572) {
            read = storage_file_read(file, buffer, size);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    if(read == 0 || read != size) return false;

    memset(app->tag, 0, sizeof(app->tag));
    size_t copy = read > AMIIBO_TAG_SIZE ? AMIIBO_TAG_SIZE : read;
    memcpy(app->tag, buffer, copy);
    app->bin_size = read;
    if(read == 572) memcpy(app->bin_suffix, buffer + AMIIBO_TAG_SIZE, 32);
    if(read == 520) {
        amiibo_fill_blank_trailer(app->tag);
        amiibo_apply_password(app->tag);
    }
    return true;
}

static bool read_nfc(App* app, const char* path) {
    if(!nfc_device_load(app->device, path)) return false;
    if(nfc_device_get_protocol(app->device) != NfcProtocolMfUltralight) return false;
    const MfUltralightData* data = nfc_device_get_data(app->device, NfcProtocolMfUltralight);
    if(data->type != MfUltralightTypeNTAG215 || data->pages_total != 135 ||
       data->pages_read != 135)
        return false;
    memset(app->tag, 0, sizeof(app->tag));
    tag_from_pages(app, data);
    app->bin_size = 0;
    return true;
}

static void set_policy_from_path(App* app) {
    const char* path = furi_string_get_cstr(app->path);
    if(path_has_dir(path, "ZE"))
        app->policy = UidForceOn;
    else if(path_has_dir(path, "SSB"))
        app->policy = UidForceOff;
    else
        app->policy = UidFollowSetting;
}

static void refresh_tag_text(App* app) {
    FuriString* name = furi_string_alloc();
    path_extract_filename(app->path, name, false);
    uint8_t uid[7];
    char uid_text[20];
    amiibo_uid_from_tag(app->tag, uid);
    format_uid(uid, uid_text, sizeof(uid_text));

    furi_string_printf(
        app->info,
        "\e#%s\nUID %s\n%s\nKeys: %s%s",
        furi_string_get_cstr(name),
        uid_text,
        policy_label(app),
        app->keys_loaded ? "loaded" : "missing",
        app->dirty ? "\nUnsaved" : "");
    furi_string_free(name);
}

static void show_tag(App* app) {
    refresh_tag_text(app);
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 48, furi_string_get_cstr(app->info));
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "New UID", widget_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Emulate", widget_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Save", widget_callback, app);
    app->current = ViewTag;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
}

static void show_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Browse dumps", MenuBrowse, submenu_callback, app);
    if(app->loaded) {
        submenu_add_item(app->submenu, "Emulate", MenuEmulate, submenu_callback, app);
        submenu_add_item(
            app->submenu, app->dirty ? "Save dump*" : "Save dump", MenuSave, submenu_callback, app);
    }
    submenu_add_item(
        app->submenu,
        app->auto_uid ? "Auto UID: ON" : "Auto UID: OFF",
        MenuAuto,
        submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        app->keys_loaded ? "Keys: loaded" : "Keys: missing",
        MenuKeys,
        submenu_callback,
        app);
    submenu_set_selected_item(app->submenu, app->menu_index);
    app->current = ViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
}

static bool assign_uid(App* app) {
    if(!app->keys_loaded) {
        app_alert(app, "Key file needed", "Copy key_retail.bin\ninto the app folder.");
        return false;
    }
    uint8_t uid[7];
    uint8_t current[7];
    amiibo_uid_from_tag(app->tag, current);
    for(int attempt = 0; attempt < 4; attempt++) {
        make_uid(uid);
        if(memcmp(uid, current, sizeof(uid)) != 0) break;
    }
    AmiiboResult result = amiibo_set_uid(app->tag, &app->keys, uid);
    if(result != AmiiboOk) {
        app_alert(app, "Not signed", "The dump did not\nmatch the key file.");
        return false;
    }
    app->dirty = true;
    return true;
}

static void stop_emulate(App* app) {
    if(!app->emulating) return;
    nfc_listener_stop(app->listener);
    const MfUltralightData* live = nfc_listener_get_data(app->listener, NfcProtocolMfUltralight);
    tag_from_pages(app, live);
    nfc_device_set_data(app->device, NfcProtocolMfUltralight, live);
    nfc_listener_free(app->listener);
    app->listener = NULL;
    nfc_free(app->nfc);
    app->nfc = NULL;
    app->emulating = false;
    app->dirty = true;
    notification_message(app->notifications, &sequence_blink_stop);
}

static bool start_emulate(App* app, bool renew) {
    if(!app->loaded) {
        app_alert(app, "No dump", "Browse for a .bin\nor .nfc file.");
        return false;
    }
    if((renew || auto_uid_enabled(app)) && !assign_uid(app)) return false;

    pages_from_tag(app);
    app->nfc = nfc_alloc();
    const MfUltralightData* data = nfc_device_get_data(app->device, NfcProtocolMfUltralight);
    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(app->listener, NULL, NULL);
    app->emulating = true;

    uint8_t uid[7];
    char uid_text[24];
    amiibo_uid_from_tag(app->tag, uid);
    format_uid(uid, uid_text, sizeof(uid_text));
    FuriString* name = furi_string_alloc();
    path_extract_filename(app->path, name, true);
    furi_string_printf(
        app->info,
        "\e#Emulating\n%s\n%s\nHold on the reader",
        furi_string_get_cstr(name),
        uid_text);
    furi_string_free(name);

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 48, furi_string_get_cstr(app->info));
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Next ID", widget_callback, app);
    app->current = ViewEmulate;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
    notification_message(app->notifications, &sequence_blink_start_magenta);
    return true;
}

static bool save_dump(App* app) {
    if(!app->loaded) return false;
    const char* path = furi_string_get_cstr(app->path);
    bool is_nfc = ends_with_i(path, ".nfc");
    bool is_bin = ends_with_i(path, ".bin");
    if(!is_nfc && !is_bin) return false;
    if(is_bin && app->bin_size != 520 && app->bin_size != 540 && app->bin_size != 572)
        return false;

    FuriString* temp = furi_string_alloc_printf("%s.amiibo.tmp", path);
    FuriString* backup = furi_string_alloc_printf("%s.amiibo.bak", path);
    const char* temp_path = furi_string_get_cstr(temp);
    const char* backup_path = furi_string_get_cstr(backup);
    bool saved = false;
    bool created = false;
    bool recovery = false;
    do {
        if(storage_common_exists(app->storage, temp_path) ||
           storage_common_exists(app->storage, backup_path)) {
            app_alert(
                app, "Recovery files", "Check .amiibo.tmp\nand .amiibo.bak\nbeside the dump.");
            break;
        }
        if(is_nfc) {
            pages_from_tag(app);
            created = true;
            if(!nfc_device_save(app->device, temp_path)) break;
            NfcDevice* verify = nfc_device_alloc();
            bool valid = nfc_device_load(verify, temp_path) &&
                         nfc_device_get_protocol(verify) == NfcProtocolMfUltralight &&
                         mf_ultralight_is_equal(
                             nfc_device_get_data(app->device, NfcProtocolMfUltralight),
                             nfc_device_get_data(verify, NfcProtocolMfUltralight));
            nfc_device_free(verify);
            if(!valid) break;
        } else {
            File* file = storage_file_alloc(app->storage);
            bool ok = storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_NEW);
            created = ok;
            size_t length = app->bin_size < AMIIBO_TAG_SIZE ? app->bin_size : AMIIBO_TAG_SIZE;
            if(ok) ok = storage_file_write(file, app->tag, length) == length;
            if(ok && app->bin_size == 572)
                ok = storage_file_write(file, app->bin_suffix, 32) == 32;
            if(ok) ok = storage_file_sync(file);
            bool closed = storage_file_close(file);
            storage_file_free(file);
            if(!ok || !closed) break;
            App verify = *app;
            if(!read_bin(&verify, temp_path) || verify.bin_size != app->bin_size ||
               memcmp(verify.tag, app->tag, length) != 0 ||
               (app->bin_size == 572 && memcmp(verify.bin_suffix, app->bin_suffix, 32) != 0))
                break;
        }
        if(storage_common_rename_safe(app->storage, path, backup_path) != FSE_OK) break;
        if(storage_common_rename_safe(app->storage, temp_path, path) != FSE_OK) {
            recovery = storage_common_rename_safe(app->storage, backup_path, path) != FSE_OK;
            break;
        }
        created = false;
        saved = true;
        if(storage_common_remove(app->storage, backup_path) != FSE_OK) {
            app_alert(app, "Saved", "Old dump retained\nas .amiibo.bak");
        }
    } while(false);
    if(created && !recovery) storage_common_remove(app->storage, temp_path);
    if(saved)
        app->dirty = false;
    else if(recovery)
        app_alert(app, "Save interrupted", "Original is in\n.amiibo.bak\nRestore it first.");
    else
        app_alert(app, "Save failed", "Original dump was\nnot replaced.");
    furi_string_free(backup);
    furi_string_free(temp);
    return saved;
}

static bool load_selected(App* app, FuriString* selected) {
    const char* path = furi_string_get_cstr(selected);
    if(!ends_with_i(path, ".bin") && !ends_with_i(path, ".nfc")) {
        app_alert(app, "Wrong file", "Choose a .bin\nor .nfc dump.");
        return false;
    }
    App candidate = *app;
    candidate.device = nfc_device_alloc();
    bool ok = ends_with_i(path, ".bin") ? read_bin(&candidate, path) : read_nfc(&candidate, path);
    if(!ok) {
        nfc_device_free(candidate.device);
        app_alert(app, "Cannot load", "Need NTAG215 NFC\nor 520/540/572\nbyte BIN dump.");
        return false;
    }
    nfc_device_free(app->device);
    app->device = candidate.device;
    memcpy(app->tag, candidate.tag, sizeof(app->tag));
    memcpy(app->bin_suffix, candidate.bin_suffix, sizeof(app->bin_suffix));
    app->bin_size = candidate.bin_size;
    furi_string_set(app->path, selected);
    app->loaded = true;
    app->dirty = false;
    set_policy_from_path(app);
    return true;
}

static void browse(App* app) {
    FuriString* selected = furi_string_alloc_set(app->path);
    if(furi_string_empty(selected)) furi_string_set(selected, APP_DATA_PATH("db"));
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".bin|.nfc", NULL);
    options.base_path = "/ext";
    options.hide_dot_files = true;
    if(dialog_file_browser_show(app->dialogs, selected, selected, &options) &&
       load_selected(app, selected)) {
        show_tag(app);
    }
    furi_string_free(selected);
}

static void widget_callback(GuiButtonType button, InputType type, void* context) {
    App* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->dispatcher, button + 100);
}

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    app->menu_index = index;
    view_dispatcher_send_custom_event(app->dispatcher, index);
}

static bool custom_event(void* context, uint32_t event) {
    App* app = context;
    if(app->emulating && event == GuiButtonTypeCenter + 100) {
        stop_emulate(app);
        if(!start_emulate(app, true)) show_tag(app);
        return true;
    }
    if(app->emulating) return true;

    if(event == MenuBrowse) {
        browse(app);
    } else if(event == MenuEmulate) {
        start_emulate(app, false);
    } else if(event == MenuAuto) {
        app->auto_uid = !app->auto_uid;
        save_settings(app);
        show_menu(app);
    } else if(event == MenuSave) {
        save_dump(app);
        show_menu(app);
    } else if(event == MenuKeys) {
        if(app->keys_loaded) {
            app_alert(app, "Keys loaded", app->key_path);
        } else {
            app_alert(app, "Keys missing", "Put key_retail.bin\nin apps_data/\nflipper_amiibo");
        }
    } else if(event == GuiButtonTypeLeft + 100) {
        if(assign_uid(app)) show_tag(app);
    } else if(event == GuiButtonTypeCenter + 100) {
        start_emulate(app, false);
    } else if(event == GuiButtonTypeRight + 100) {
        if(save_dump(app)) show_tag(app);
    }
    return true;
}

static bool back_event(void* context) {
    App* app = context;
    if(app->emulating || app->current == ViewEmulate) {
        stop_emulate(app);
        show_tag(app);
        return true;
    }
    if(app->current == ViewTag) {
        show_menu(app);
        return true;
    }
    return false;
}

int32_t flipper_amiibo_app(void* arg) {
    UNUSED(arg);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->auto_uid = true;

    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    storage_simply_mkdir(app->storage, "/ext/apps_data/flipper_amiibo");
    storage_simply_mkdir(app->storage, APP_DATA_PATH("db"));
    load_settings(app);
    load_keys(app);

    app->path = furi_string_alloc_set(APP_DATA_PATH("db"));
    app->info = furi_string_alloc();
    app->device = nfc_device_alloc();
    app->dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, custom_event);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back_event);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_add_view(app->dispatcher, ViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->dispatcher, ViewTag, widget_get_view(app->widget));

    show_menu(app);
    view_dispatcher_run(app->dispatcher);

    if(app->emulating) stop_emulate(app);

    view_dispatcher_remove_view(app->dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->dispatcher, ViewTag);
    widget_free(app->widget);
    submenu_free(app->submenu);
    view_dispatcher_free(app->dispatcher);
    nfc_device_free(app->device);
    furi_string_free(app->info);
    furi_string_free(app->path);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
