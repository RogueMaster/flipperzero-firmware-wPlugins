/*
 * RFIDduplicates v1.0 - created by Marcopiiit
 *
 * Main menu:
 *   - Read              : reads a 125 kHz RFID tag and, if it is saved in
 *                         /ext/lfrfid, shows the name(s) of the saved key(s)
 *   - Check duplicates  : scans every saved .rfid key and lists duplicates
 *   - About
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>
#include <storage/storage.h>
#include <toolbox/protocols/protocol_dict.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/lfrfid_dict_file.h>
#include <lfrfid/protocols/lfrfid_protocols.h>

#define LFRFID_FOLDER  EXT_PATH("lfrfid")
#define LFRFID_EXT     ".rfid"
#define MAX_KEYS       256
#define KEY_NAME_LEN   40
#define KEY_DATA_MAX   32

typedef enum {
    ViewMenu,
    ViewLoading,
    ViewRead,
    ViewDups,
    ViewAbout,
} AppView;

typedef enum {
    MenuRead,
    MenuDuplicates,
    MenuAbout,
} MenuItem;

typedef enum {
    ModeRead,
    ModeDuplicates,
} ScanMode;

typedef enum {
    EventScanDone = 1,
    EventTagRead,
    EventReadAgain,
} AppEvent;

typedef struct {
    char name[KEY_NAME_LEN];
    ProtocolId protocol;
    uint8_t size;
    uint8_t data[KEY_DATA_MAX];
} SavedKey;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Loading* loading;
    Widget* widget_read;
    Widget* widget_dups;
    Widget* widget_about;

    ProtocolDict* dict;
    LFRFIDWorker* worker;
    FuriThread* thread;

    AppView current;
    ScanMode mode;

    SavedKey* keys;
    size_t key_count;
    bool truncated;

    FuriString* text; // result text for the duplicates screen

    ProtocolId read_protocol;
    uint8_t read_size;
    uint8_t read_data[KEY_DATA_MAX];
} App;

/* ------------------------------------------------------------------------- */
/* Saved keys loading                                                         */
/* ------------------------------------------------------------------------- */

static void load_keys(App* app) {
    app->key_count = 0;
    app->truncated = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    FuriString* name = furi_string_alloc();
    char fname[128];
    FileInfo info;

    if(storage_dir_open(dir, LFRFID_FOLDER)) {
        while(storage_dir_read(dir, &info, fname, sizeof(fname))) {
            if(file_info_is_dir(&info)) continue;

            furi_string_set(name, fname);
            if(!furi_string_end_with_str(name, LFRFID_EXT)) continue;

            if(app->key_count >= MAX_KEYS) {
                app->truncated = true;
                break;
            }

            furi_string_printf(path, "%s/%s", LFRFID_FOLDER, fname);
            ProtocolId id = lfrfid_dict_file_load(app->dict, furi_string_get_cstr(path));
            if(id == PROTOCOL_NO) continue;

            size_t size = protocol_dict_get_data_size(app->dict, id);
            if(size == 0 || size > KEY_DATA_MAX) continue;

            SavedKey* key = &app->keys[app->key_count];
            protocol_dict_get_data(app->dict, id, key->data, size);
            key->protocol = id;
            key->size = (uint8_t)size;

            furi_string_left(name, furi_string_size(name) - strlen(LFRFID_EXT));
            strlcpy(key->name, furi_string_get_cstr(name), KEY_NAME_LEN);

            app->key_count++;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_string_free(path);
    furi_string_free(name);
    furi_record_close(RECORD_STORAGE);
}

static bool keys_equal(const SavedKey* a, const SavedKey* b) {
    return a->protocol == b->protocol && a->size == b->size &&
           memcmp(a->data, b->data, a->size) == 0;
}

static void build_duplicates_text(App* app) {
    furi_string_reset(app->text);

    if(app->key_count == 0) {
        furi_string_set(app->text, "\e#No saved RFID tags\e#\nNothing found in /ext/lfrfid");
        return;
    }

    bool* visited = malloc(sizeof(bool) * MAX_KEYS);
    memset(visited, 0, sizeof(bool) * MAX_KEYS);

    FuriString* body = furi_string_alloc();
    size_t groups = 0;

    for(size_t i = 0; i < app->key_count; i++) {
        if(visited[i]) continue;
        size_t members = 1;
        for(size_t j = i + 1; j < app->key_count; j++) {
            if(visited[j]) continue;
            if(keys_equal(&app->keys[i], &app->keys[j])) {
                if(members == 1) {
                    groups++;
                    furi_string_cat_printf(body, "%u) %s", (unsigned)groups, app->keys[i].name);
                }
                furi_string_cat_printf(body, " = %s", app->keys[j].name);
                visited[j] = true;
                members++;
            }
        }
        if(members > 1) furi_string_cat(body, "\n");
    }

    furi_string_printf(app->text, "\e#Tags checked: %u\e#\n", (unsigned)app->key_count);
    if(groups == 0) {
        furi_string_cat(app->text, "No duplicates found.");
    } else {
        furi_string_cat_printf(app->text, "Duplicate groups: %u\n\n", (unsigned)groups);
        furi_string_cat(app->text, body);
    }
    if(app->truncated) {
        furi_string_cat_printf(app->text, "\n(Limit of %d tags reached)", MAX_KEYS);
    }

    furi_string_free(body);
    free(visited);
}

/* ------------------------------------------------------------------------- */
/* Background scan thread                                                     */
/* ------------------------------------------------------------------------- */

static int32_t scan_thread(void* context) {
    App* app = context;
    load_keys(app);
    if(app->mode == ModeDuplicates) {
        build_duplicates_text(app);
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, EventScanDone);
    return 0;
}

static void start_scan(App* app, ScanMode mode) {
    app->mode = mode;
    app->current = ViewLoading;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);

    app->thread = furi_thread_alloc_ex("RfidDupScan", 6 * 1024, scan_thread, app);
    furi_thread_start(app->thread);
}

/* ------------------------------------------------------------------------- */
/* Read                                                                       */
/* ------------------------------------------------------------------------- */

static void read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    App* app = context;
    if(result == LFRFIDWorkerReadDone && protocol != PROTOCOL_NO) {
        size_t size = protocol_dict_get_data_size(app->dict, protocol);
        if(size > KEY_DATA_MAX) size = KEY_DATA_MAX;
        protocol_dict_get_data(app->dict, protocol, app->read_data, size);
        app->read_protocol = protocol;
        app->read_size = (uint8_t)size;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventTagRead);
    }
}

static void start_read(App* app) {
    widget_reset(app->widget_read);
    widget_add_string_multiline_element(
        app->widget_read, 64, 18, AlignCenter, AlignCenter, FontPrimary, "Reading RFID...");
    widget_add_string_multiline_element(
        app->widget_read,
        64,
        42,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Place the tag on the\nback of the Flipper");

    app->current = ViewRead;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewRead);

    lfrfid_worker_read_start(app->worker, LFRFIDWorkerReadTypeAuto, read_callback, app);
}

static void read_button_callback(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventReadAgain);
    }
}

static void show_read_result(App* app) {
    lfrfid_worker_stop(app->worker);

    FuriString* text = furi_string_alloc();
    size_t matches = 0;

    for(size_t i = 0; i < app->key_count; i++) {
        SavedKey* key = &app->keys[i];
        if(key->protocol == app->read_protocol && key->size == app->read_size &&
           memcmp(key->data, app->read_data, key->size) == 0) {
            if(matches == 0) furi_string_cat(text, "\e#Saved as:\e#\n");
            furi_string_cat_printf(text, "%s\n", key->name);
            matches++;
        }
    }
    if(matches == 0) {
        furi_string_cat(text, "\e#Tag not in memory\e#\n");
    }

    furi_string_cat_printf(
        text, "\n%s\n", protocol_dict_get_name(app->dict, app->read_protocol));
    for(size_t i = 0; i < app->read_size; i++) {
        furi_string_cat_printf(text, "%02X ", app->read_data[i]);
    }

    widget_reset(app->widget_read);
    widget_add_text_box_element(
        app->widget_read, 0, 0, 128, 52, AlignLeft, AlignTop, furi_string_get_cstr(text), false);
    widget_add_button_element(
        app->widget_read, GuiButtonTypeCenter, "Again", read_button_callback, app);

    furi_string_free(text);
}

/* ------------------------------------------------------------------------- */
/* Screens                                                                    */
/* ------------------------------------------------------------------------- */

static void show_duplicates(App* app) {
    widget_reset(app->widget_dups);
    widget_add_text_box_element(
        app->widget_dups,
        0,
        0,
        128,
        64,
        AlignLeft,
        AlignTop,
        furi_string_get_cstr(app->text),
        false);
    app->current = ViewDups;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewDups);
}

static void setup_about(App* app) {
    widget_reset(app->widget_about);
    widget_add_text_box_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        AlignCenter,
        AlignCenter,
        "\e#v1.0\e#\nCreated by Marcopiiit",
        false);
}

/* ------------------------------------------------------------------------- */
/* Callbacks                                                                  */
/* ------------------------------------------------------------------------- */

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case MenuRead:
        start_scan(app, ModeRead);
        break;
    case MenuDuplicates:
        start_scan(app, ModeDuplicates);
        break;
    case MenuAbout:
        app->current = ViewAbout;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewAbout);
        break;
    }
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    switch(event) {
    case EventScanDone:
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
        if(app->mode == ModeDuplicates) {
            show_duplicates(app);
        } else {
            start_read(app);
        }
        return true;
    case EventTagRead:
        show_read_result(app);
        return true;
    case EventReadAgain:
        start_read(app);
        return true;
    }
    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;
    switch(app->current) {
    case ViewMenu:
        return false; // exit the app
    case ViewLoading:
        return true; // ignore Back while scanning
    case ViewRead:
        lfrfid_worker_stop(app->worker);
        /* fallthrough */
    default:
        app->current = ViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    }
}

/* ------------------------------------------------------------------------- */
/* App lifecycle                                                              */
/* ------------------------------------------------------------------------- */

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->keys = malloc(sizeof(SavedKey) * MAX_KEYS);
    app->text = furi_string_alloc();
    app->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    app->worker = lfrfid_worker_alloc(app->dict);
    lfrfid_worker_start_thread(app->worker);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "RFIDduplicates");
    submenu_add_item(app->submenu, "Read", MenuRead, submenu_callback, app);
    submenu_add_item(app->submenu, "Check duplicates", MenuDuplicates, submenu_callback, app);
    submenu_add_item(app->submenu, "About", MenuAbout, submenu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, ViewMenu, submenu_get_view(app->submenu));

    app->loading = loading_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewLoading, loading_get_view(app->loading));

    app->widget_read = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewRead, widget_get_view(app->widget_read));

    app->widget_dups = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewDups, widget_get_view(app->widget_dups));

    app->widget_about = widget_alloc();
    setup_about(app);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewAbout, widget_get_view(app->widget_about));

    app->current = ViewMenu;
    return app;
}

static void app_free(App* app) {
    lfrfid_worker_stop(app->worker);
    lfrfid_worker_stop_thread(app->worker);
    lfrfid_worker_free(app->worker);
    protocol_dict_free(app->dict);

    view_dispatcher_remove_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, ViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, ViewDups);
    view_dispatcher_remove_view(app->view_dispatcher, ViewAbout);

    submenu_free(app->submenu);
    loading_free(app->loading);
    widget_free(app->widget_read);
    widget_free(app->widget_dups);
    widget_free(app->widget_about);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->text);
    free(app->keys);
    free(app);
}

int32_t rfid_duplicates_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
