#include "amiibo_crypto.h"
#include "amiibo_names.h"
#include "menu_icons.h"
#include "splash_icon.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_stack.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>
#include <toolbox/path.h>

typedef enum {
    ViewMenu,
    ViewTag,
    ViewEmulate,
    ViewHelp,
    ViewAbout,
    ViewRead,
    ViewSettings,
    ViewMy,
    ViewSaved,
    ViewUnsaved,
    ViewCarousel,
    ViewSplash,
    ViewDatabase,
    ViewCleanup,
    ViewNotice,
    ViewBlast,
} ViewId;

typedef enum {
    MenuUnsaved = 1,
    MenuRead,
    MenuVersion,
    MenuAuto,
    MenuKeys,
    MenuSort,
    MenuDeleteScanned,
    MenuDeleteFavorites,
    MenuEmpty,
} MenuId;

#define FILE_MAX     40
/* One database folder keeps this many names. Each name is stored twice (the
 * folder list and the on-screen row), and a few hundred of those can exhaust
 * the heap. Favorites and Scanned stay at FILE_MAX. */
#define DIR_LIST_MAX 200
#define MenuFileBase 400

#define APP_VERSION "1.2"

typedef enum {
    UidFollowSetting,
    UidForceOn,
    UidForceOff,
} UidPolicy;

typedef enum {
    OriginDatabase,
    OriginSaved,
    OriginUnsaved,
    OriginBlast,
} TagOrigin;

typedef enum {
    BrowseNone,
    BrowseDatabase,
    BrowseSaved,
    BrowseUnsaved,
    BrowseBlast,
} BrowseFrom;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    View* list_view;
    Widget* widget;
    View* menu_view;
    View* splash_view;
    View* cleanup_view;
    FuriTimer* menu_timer;
    View* help_view;
    View* about_view;
    View* tag_buttons;
    ViewStack* tag_stack;
    Popup* popup;
    bool uid_confirm;
    FuriPubSub* input_pubsub;
    FuriPubSubSubscription* input_subscription;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Storage* storage;
    FuriString* path;
    FuriString* info;

    NfcDevice* device;
    Nfc* nfc;
    NfcListener* listener;
    NfcPoller* poller;

    uint8_t tag[AMIIBO_TAG_SIZE];
    uint8_t bin_suffix[32];
    size_t bin_size;
    AmiiboKeys keys;
    bool loaded;
    bool dirty;
    bool auto_uid;
    bool sort_series;
    bool list_cut;
    bool cut_notice;
    ViewId notice_return;
    bool in_series;
    bool in_other;
    bool blast_list;
    bool blasting;
    bool blast_on_air;
    uint32_t blast_phase_started;
    uint32_t blast_index;
    uint8_t blast_saved[AMIIBO_TAG_SIZE];
    uint8_t open_series;
    uint8_t series_ids[32];
    uint8_t series_other[32];
    uint8_t series_shown;
    uint32_t series_index;
    bool keys_loaded;
    bool emulating;
    bool reading;
    bool figure_read;
    uint32_t read_started;
    UidPolicy policy;
    TagOrigin origin;
    BrowseFrom browse;
    ViewId current;
    char key_path[96];
    uint32_t my_index;
    uint32_t settings_index;
    uint32_t saved_index;
    uint32_t unsaved_index;
    FuriString* files[FILE_MAX];
    uint8_t file_count;
    FuriString* db_path;
    FuriString** db_names;
    uint8_t* db_is_dir;
    uint16_t db_count;
    uint16_t db_capacity;
    uint32_t db_index;
    uint32_t db_stack[8];
    uint8_t db_depth;
    bool cleanup_holding;
    uint32_t cleanup_hold_start;
    uint32_t splash_started;
} App;

static const char HELP[] = "This app can back up\n"
                           "Amiibos you own.\n"
                           "\n"
                           "You do not need a\n"
                           "downloaded Amiibo\n"
                           "database. You do not\n"
                           "need a key file.\n"
                           "\n"
                           "Scan a real Amiibo.\n"
                           "It stays under\n"
                           "Scanned Amiibos.\n"
                           "Fav keeps a copy in\n"
                           "Favorites.\n"
                           "\n"
                           "Press Emulate. Hold\n"
                           "the Flipper on the\n"
                           "game reader. The game\n"
                           "sees that same\n"
                           "Amiibo.\n"
                           "\n"
                           "Amiibo Blast lists\n"
                           "your Favorites. Blast\n"
                           "sends a new ID for\n"
                           "1 second, pauses\n"

                           "2 seconds, and repeats.\n"
                           "It does not save that\n"
                           "ID. Any key stops.\n"
                           "This needs the key\n"
                           "file.\n"
                           "\n"
                           "Key\n"
                           "The app cannot include\n"
                           "Nintendo's key file.\n"
                           "You add that file\n"
                           "yourself.\n"
                           "\n"
                           "New UID and Auto UID\n"
                           "cannot change the ID\n"
                           "without that key on\n"
                           "the SD card.\n"
                           "\n"
                           "Emulate still works\n"
                           "without the key. It\n"
                           "uses the ID from\n"
                           "your Amiibo.\n"
                           "\n"
                           "If you have the file,\n"
                           "copy key_retail.bin\n"
                           "into:\n"
                           "apps_data/amiibo_emulator\n"
                           "\n"
                           "If amiibo_keys.bin is\n"
                           "already in the nfc\n"
                           "folder, you can leave\n"
                           "it there.\n"
                           "\n"
                           "Open Settings to see\n"
                           "if the key is ready.\n"
                           "\n"
                           "Scan\n"
                           "Hold your real Amiibo\n"
                           "on the back of the\n"
                           "Flipper.\n"
                           "\n"
                           "Its name is already\n"
                           "on it. You do not\n"
                           "type the name.\n"
                           "\n"
                           "Fav keeps a copy in\n"
                           "Favorites. The ID\n"
                           "stays the same.\n"
                           "\n"
                           "On a favorite, New UID\n"
                           "asks, then saves the\n"
                           "new ID on that\n"
                           "favorite. This needs\n"
                           "the key file.\n"
                           "\n"
                           "If a scan does not\n"
                           "work, press OK to try\n"
                           "again. Back returns\n"
                           "to the Scan menu.\n"
                           "\n"
                           "Database\n"
                           "A downloaded database\n"
                           "is optional. Put\n"
                           "files in:\n"
                           "apps_data/amiibo_emulator/db\n"
                           "\n"
                           "Folders are listed\n"
                           "first, then files,\n"
                           "A to Z.\n"
                           "\n"
                           "Only Amiibo .bin and\n"
                           "NTAG215 .nfc files\n"
                           "are listed. A long\n"
                           "folder shows the first\n"
                           "200 and says so.\n"
                           "Favorites and scans\n"
                           "show the first 40.\n"
                           "\n"
                           "A folder named ZE\n"
                           "always makes a new ID.\n"
                           "A folder named SSB\n"
                           "keeps the ID saved in\n"
                           "the file.\n"
                           "\n"
                           "Open an Amiibo and\n"
                           "press Emulate.\n"
                           "Fav keeps a copy in\n"
                           "Favorites.\n"
                           "\n"
                           "Settings\n"
                           "Auto UID, when on,\n"
                           "makes a new ID each\n"
                           "time you emulate.\n"
                           "This needs the key\n"
                           "file. Next ID makes\n"
                           "another one on that\n"
                           "screen.\n"
                           "\n"
                           "Sort lists chooses\n"
                           "Unsorted or By Series\n"
                           "for Favorites and\n"
                           "Scanned Amiibos.\n"
                           "\n"
                           "Delete Scanned Amiibos\n"
                           "removes scans only.\n"
                           "Delete Favorites\n"
                           "removes favorites only.\n"
                           "Hold Up for\n"

                           "2 seconds. The\n"
                           "downloaded database\n"
                           "stays.\n";

static const char ABOUT[] = "Amiibo is a trademark\n"
                            "of Nintendo.\n"
                            "\n"
                            "Characters and names\n"
                            "are copyright Nintendo.\n"
                            "All rights reserved.\n"
                            "\n"
                            "This app is not made\n"
                            "by Nintendo.\n"
                            "\n"
                            "Written by\n"
                            "Soap-n-Cartridge.\n"
                            "Open source.\n"
                            "\n"
                            "Does not own Amiibo\n"
                            "or Nintendo content.\n";

typedef struct {
    uint16_t scroll;
    uint16_t lines;
    uint8_t visible;
    const char* title;
    const char* body;
} PageModel;

static uint16_t page_line_count(const char* text) {
    uint16_t count = 0;
    while(*text) {
        count++;
        while(*text && *text != '\n')
            text++;
        if(*text == '\n') text++;
    }
    return count;
}

static void page_copy_line(const char* text, uint16_t index, char* out, size_t out_size) {
    uint16_t line = 0;
    while(*text && line < index) {
        if(*text == '\n') line++;
        text++;
    }
    size_t used = 0;
    while(*text && *text != '\n' && used + 1 < out_size)
        out[used++] = *text++;
    out[used] = '\0';
}

static void page_draw(Canvas* canvas, void* context) {
    PageModel* model = context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 11, model->title);

    canvas_set_font(canvas, FontSecondary);
    size_t font_height = canvas_current_font_height(canvas);
    if(font_height == 0) font_height = 10;
    uint8_t visible = (uint8_t)((64 - 16) / font_height);
    if(visible == 0) visible = 1;
    model->visible = visible;
    uint16_t max_scroll = model->lines > visible ? model->lines - visible : 0;
    if(model->scroll > max_scroll) model->scroll = max_scroll;

    char line[40];
    for(uint8_t row = 0; row < visible; row++) {
        uint16_t index = model->scroll + row;
        if(index >= model->lines) break;
        page_copy_line(model->body, index, line, sizeof(line));
        canvas_draw_str(canvas, 4, (int32_t)(16 + ((row + 1) * font_height) - 2), line);
    }
    if(model->lines > visible) elements_scrollbar(canvas, model->scroll, model->lines);
}

static bool page_input(InputEvent* event, View* view) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key != InputKeyUp && event->key != InputKeyDown) return false;

    PageModel* model = view_get_model(view);
    uint8_t visible = model->visible ? model->visible : 4;
    uint16_t max_scroll = model->lines > visible ? model->lines - visible : 0;
    if(event->key == InputKeyDown && model->scroll < max_scroll) model->scroll++;
    if(event->key == InputKeyUp && model->scroll > 0) model->scroll--;
    view_commit_model(view, true);
    return true;
}

static bool help_input(InputEvent* event, void* context) {
    App* app = context;
    return page_input(event, app->help_view);
}

static bool about_input(InputEvent* event, void* context) {
    App* app = context;
    return page_input(event, app->about_view);
}

static void show_about(App* app) {
    PageModel* model = view_get_model(app->about_view);
    model->scroll = 0;
    view_commit_model(app->about_view, false);
    app->current = ViewAbout;
    view_dispatcher_switch_to_view(app->dispatcher, ViewAbout);
}

static void show_menu(App* app);
static void show_tag(App* app);
static void blast_stop(App* app);
static void show_database(App* app);
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

static void format_uid_display(const uint8_t uid[7], char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%02X:%02X:%02X:%02X:%02X:%02X:%02X",
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
    if(app->origin == OriginSaved) return "Favorite UID kept";
    if(app->origin == OriginUnsaved) return "Scanned";
    if(app->figure_read) return "Figure UID kept";
    if(app->policy == UidForceOn) return "ZE: new ID each scan";
    if(app->policy == UidForceOff) return "SSB: file ID kept";
    return app->auto_uid ? "Auto UID: on" : "Auto UID: off";
}

static void save_settings(App* app);

static void load_settings(App* app) {
    app->auto_uid = true;
    app->sort_series = false;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, APP_DATA_PATH("auto_uid"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char value[3];
        size_t read = storage_file_read(file, value, sizeof(value));
        if(read >= 1) app->auto_uid = value[0] != '0';
        if(read >= 2) app->sort_series = value[1] == '1';
        storage_file_close(file);
        if(read > 2) save_settings(app);
    }
    storage_file_free(file);
}

static void save_settings(App* app) {
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, APP_DATA_PATH("auto_uid"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char value[2];
        value[0] = app->auto_uid ? '1' : '0';
        value[1] = app->sort_series ? '1' : '0';
        storage_file_write(file, value, sizeof(value));
        storage_file_close(file);
    }
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

static void append_wrapped(FuriString* out, const char* text) {
    const size_t width = 20;
    size_t len = strlen(text);
    if(len <= width) {
        furi_string_cat(out, text);
        return;
    }
    size_t split = width;
    while(split > 8 && text[split] != ' ')
        split--;
    if(text[split] != ' ') split = width;
    size_t rest = split;
    if(text[split] == ' ') rest++;
    furi_string_cat_printf(out, "%.*s\n%s", (int)split, text, text + rest);
}

static void refresh_tag_text(App* app) {
    FuriString* name = furi_string_alloc();
    path_extract_filename(app->path, name, false);
    uint8_t uid[7];
    char uid_text[24];
    amiibo_uid_from_tag(app->tag, uid);
    format_uid_display(uid, uid_text, sizeof(uid_text));
    const char* figure = amiibo_figure_name(app->tag);

    furi_string_set(app->info, "\e#");
    append_wrapped(app->info, figure ? figure : furi_string_get_cstr(name));
    furi_string_cat(app->info, "\e#");
    furi_string_cat_printf(
        app->info, "\nUID %s\n%s%s", uid_text, policy_label(app), app->dirty ? "\nChanged" : "");
    furi_string_free(name);
}

static void set_read_path(App* app) {
    uint8_t uid[7];
    amiibo_uid_from_tag(app->tag, uid);
    const char* figure = amiibo_figure_name(app->tag);
    char safe[44];
    size_t n = 0;
    if(figure) {
        for(size_t i = 0; figure[i] != '\0' && n + 1 < sizeof(safe); i++) {
            char c = figure[i];
            bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                      c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')' ||
                      c == '#';
            if(ok) safe[n++] = c;
        }
        while(n > 0 && safe[n - 1] == ' ')
            n--;
    }
    safe[n] = '\0';
    if(n == 0) {
        char uid_text[20];
        format_uid(uid, uid_text, sizeof(uid_text));
        furi_string_printf(app->path, "%s/%s.bin", APP_DATA_PATH("scanned"), uid_text);
        return;
    }
    furi_string_printf(
        app->path,
        "%s/%s_%02X%02X%02X.bin",
        APP_DATA_PATH("scanned"),
        safe,
        uid[4],
        uid[5],
        uid[6]);
}

typedef struct {
    App* app;
} TagBarModel;

static uint8_t tag_button_width(Canvas* canvas, const char* text, bool center) {
    size_t width = canvas_string_width(canvas, text) + (center ? 12 : 13);
    if(width > 128) width = 128;
    return (uint8_t)width;
}

static void tag_button_draw(Canvas* canvas, int x, uint8_t width, const char* text, char side) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, x, 52, width, 12);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontSecondary);
    if(side == 'L') {
        canvas_draw_line(canvas, x + 6, 54, x + 3, 57);
        canvas_draw_line(canvas, x + 3, 57, x + 6, 60);
        canvas_draw_str(canvas, x + 10, 61, text);
    } else if(side == 'R') {
        canvas_draw_str(canvas, x + 3, 61, text);
        int tip = x + width - 6;
        canvas_draw_line(canvas, tip, 54, tip + 3, 57);
        canvas_draw_line(canvas, tip + 3, 57, tip, 60);
    } else {
        canvas_draw_disc(canvas, x + 4, 57, 3);
        canvas_draw_str(canvas, x + 11, 61, text);
    }
    canvas_invert_color(canvas);
}

static void tag_buttons_draw(Canvas* canvas, void* context) {
    TagBarModel* model = context;
    App* app = model->app;
    if(app->current != ViewTag || app->uid_confirm) return;

    if(app->origin == OriginBlast) {
        canvas_set_font(canvas, FontSecondary);
        uint8_t mid_w = tag_button_width(canvas, "Blast", true);
        tag_button_draw(canvas, (128 - mid_w) / 2, mid_w, "Blast", 'C');
        return;
    }

    const char* left = app->origin == OriginSaved ? "New UID" : "Fav";
    const char* right = app->origin == OriginDatabase ? "New UID" : "Del";
    canvas_set_font(canvas, FontSecondary);
    uint8_t left_w = tag_button_width(canvas, left, false);
    uint8_t mid_w = tag_button_width(canvas, "Emulate", true);
    uint8_t right_w = tag_button_width(canvas, right, false);
    int gap = 128 - (int)left_w - (int)mid_w - (int)right_w;
    if(gap < 0) gap = 0;
    tag_button_draw(canvas, 0, left_w, left, 'L');
    tag_button_draw(canvas, left_w + (gap / 2), mid_w, "Emulate", 'C');
    tag_button_draw(canvas, 128 - right_w, right_w, right, 'R');
}

static bool tag_buttons_input(InputEvent* event, void* context) {
    App* app = context;
    if(app->blasting) {
        if(event->type == InputTypeShort) blast_stop(app);
        return true;
    }
    if(app->current != ViewTag || app->uid_confirm) return false;
    if(app->origin == OriginBlast) {
        if(event->key == InputKeyOk) {
            widget_callback(GuiButtonTypeCenter, event->type, app);
            return true;
        }
        return false;
    }
    if(event->key == InputKeyLeft) {
        widget_callback(GuiButtonTypeLeft, event->type, app);
        return true;
    }
    if(event->key == InputKeyOk) {
        widget_callback(GuiButtonTypeCenter, event->type, app);
        return true;
    }
    if(event->key == InputKeyRight) {
        widget_callback(GuiButtonTypeRight, event->type, app);
        return true;
    }
    return false;
}

static void show_tag(App* app) {
    app->uid_confirm = false;
    refresh_tag_text(app);
    widget_reset(app->widget);
    widget_add_text_box_element(
        app->widget, 0, 0, 128, 48, AlignLeft, AlignTop, furi_string_get_cstr(app->info), false);
    app->current = ViewTag;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
}

typedef struct {
    uint8_t index;
    int16_t offset;
    int8_t dir;
} MenuModel;

static void show_menu(App* app) {
    MenuModel* model = view_get_model(app->menu_view);
    model->offset = 0;
    model->dir = 0;
    view_commit_model(app->menu_view, true);
    furi_timer_stop(app->menu_timer);
    app->current = ViewCarousel;
    view_dispatcher_switch_to_view(app->dispatcher, ViewCarousel);
}

typedef struct {
    FuriString* label;
    unsigned count;
    bool show_count;
    uint32_t index;
} ListItem;

typedef struct {
    ListItem* items;
    uint16_t count;
    uint16_t capacity;
    FuriString* header;
    unsigned header_count;
    bool header_show;
    size_t position;
    size_t window_position;
} ListModel;

#define COUNT_RIGHT 122

static void
    list_draw_line(Canvas* canvas, const char* name, unsigned count, bool show_count, int x, int y) {
    char number[12];
    uint16_t number_w = 0;
    if(show_count && count > 0) {
        snprintf(number, sizeof(number), "%u", count);
        number_w = canvas_string_width(canvas, number);
    }
    int room = COUNT_RIGHT - x;
    if(number_w > 0) {
        if(room > (int)number_w + 4)
            room -= (int)number_w + 4;
        else
            room = 0;
    }
    FuriString* shown = furi_string_alloc_set(name);
    if(room <= 0)
        furi_string_reset(shown);
    else
        elements_string_fit_width(canvas, shown, (size_t)room);
    canvas_draw_str(canvas, x, y, furi_string_get_cstr(shown));
    furi_string_free(shown);
    if(number_w > 0) canvas_draw_str(canvas, COUNT_RIGHT - (int)number_w, y, number);
}

static void list_draw(Canvas* canvas, void* _model) {
    ListModel* model = _model;
    const uint8_t item_height = 16;
    uint8_t item_width = canvas_width(canvas) - 5;
    bool has_header = model->header && !furi_string_empty(model->header);
    size_t items_on_screen = has_header ? 3 : 4;
    uint8_t y_offset = has_header ? 16 : 0;

    canvas_clear(canvas);
    if(has_header) {
        canvas_set_font(canvas, FontPrimary);
        canvas_set_color(canvas, ColorBlack);
        list_draw_line(
            canvas,
            furi_string_get_cstr(model->header),
            model->header_count,
            model->header_show,
            4,
            11);
    }
    canvas_set_font(canvas, FontSecondary);
    for(uint16_t position = 0; position < model->count; position++) {
        if(position < model->window_position) continue;
        size_t item_position = position - model->window_position;
        if(item_position >= items_on_screen) break;
        if(position == model->position) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(
                canvas,
                0,
                y_offset + (item_position * item_height) + 1,
                item_width,
                item_height - 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        list_draw_line(
            canvas,
            furi_string_get_cstr(model->items[position].label),
            model->items[position].count,
            model->items[position].show_count,
            6,
            y_offset + (item_position * item_height) + item_height - 4);
    }
    elements_scrollbar(canvas, model->position, model->count);
}

static void list_move(ListModel* model, bool down) {
    bool has_header = model->header && !furi_string_empty(model->header);
    size_t items_on_screen = has_header ? 3 : 4;
    size_t items_size = model->count;
    if(items_size == 0) return;
    if(!down) {
        if(model->position > 0) {
            model->position--;
            if(model->position == model->window_position && model->window_position > 0) {
                model->window_position--;
            }
        } else {
            model->position = items_size - 1;
            if(model->position > items_on_screen - 1) {
                model->window_position = model->position - (items_on_screen - 1);
            }
        }
        return;
    }
    if(model->position < items_size - 1) {
        model->position++;
        if((model->position - model->window_position > items_on_screen - 2) &&
           (model->window_position < items_size - items_on_screen)) {
            model->window_position++;
        }
    } else {
        model->position = 0;
        model->window_position = 0;
    }
}

static bool list_input(InputEvent* event, void* context) {
    App* app = context;
    bool consumed = false;
    if(event->key == InputKeyOk) {
        consumed = true;
        if(event->type == InputTypeShort) {
            uint32_t index = 0;
            bool have = false;
            with_view_model(
                app->list_view,
                ListModel * model,
                {
                    if(model->position < model->count) {
                        index = model->items[model->position].index;
                        have = true;
                    }
                },
                false);
            if(have) submenu_callback(app, index);
        }
    } else if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            consumed = true;
            with_view_model(
                app->list_view,
                ListModel * model,
                { list_move(model, event->key == InputKeyDown); },
                true);
        }
    }
    return consumed;
}

static void list_reset(App* app) {
    with_view_model(
        app->list_view,
        ListModel * model,
        {
            for(uint16_t i = 0; i < model->count; i++)
                furi_string_free(model->items[i].label);
            free(model->items);
            model->items = NULL;
            model->count = 0;
            model->capacity = 0;
            model->position = 0;
            model->window_position = 0;
            furi_string_reset(model->header);
            model->header_count = 0;
            model->header_show = false;
        },
        true);
}

static void list_header(App* app, const char* name, unsigned count, bool show_count) {
    with_view_model(
        app->list_view,
        ListModel * model,
        {
            furi_string_set(model->header, name);
            model->header_count = count;
            model->header_show = show_count;
        },
        true);
}

static void list_add(App* app, const char* name, unsigned count, bool show_count, uint32_t index) {
    with_view_model(
        app->list_view,
        ListModel * model,
        {
            if(model->count == model->capacity) {
                uint16_t next = model->capacity == 0 ? 8 : (uint16_t)(model->capacity * 2);
                ListItem* grown = realloc(model->items, (size_t)next * sizeof(ListItem));
                if(grown) {
                    model->items = grown;
                    model->capacity = next;
                }
            }
            if(model->count < model->capacity) {
                ListItem* item = &model->items[model->count++];
                item->label = furi_string_alloc_set(name);
                item->count = count;
                item->show_count = show_count;
                item->index = index;
            }
        },
        true);
}

static void list_select(App* app, uint32_t index) {
    with_view_model(
        app->list_view,
        ListModel * model,
        {
            size_t position = 0;
            for(; position < model->count; position++) {
                if(model->items[position].index == index) break;
            }
            if(position >= model->count) position = 0;
            model->position = position;
            model->window_position = position;
            if(model->window_position > 0) model->window_position--;
            bool has_header = model->header && !furi_string_empty(model->header);
            size_t items_on_screen = has_header ? 3 : 4;
            if(model->count <= items_on_screen) {
                model->window_position = 0;
            } else {
                size_t pos = model->count - items_on_screen;
                if(model->window_position > pos) model->window_position = pos;
            }
        },
        true);
}

static bool bin_size_ok(uint64_t size) {
    return size == 520 || size == 540 || size == 572;
}

/* Flipper NFC text, not a full parse. Amiibo dumps are NTAG215: 135 pages. */
static bool nfc_header_is_amiibo(Storage* storage, const char* path) {
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char* buf = malloc(768);
        if(buf) {
            size_t n = storage_file_read(file, buf, 767);
            buf[n] = '\0';
            const char* pages = strstr(buf, "Pages total:");
            if(pages) {
                pages += strlen("Pages total:");
                while(*pages == ' ' || *pages == '\t')
                    pages++;
                unsigned total = 0;
                bool digits = false;
                while(*pages >= '0' && *pages <= '9') {
                    digits = true;
                    total = total * 10u + (unsigned)(*pages - '0');
                    if(total > 1000u) break;
                    pages++;
                }
                bool ultralight = strstr(buf, "NTAG215") != NULL ||
                                  strstr(buf, "Ultralight") != NULL;
                ok = digits && total == 135 && ultralight;
            } else if(strstr(buf, "NTAG215") != NULL) {
                ok = true;
            }
            free(buf);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

static bool dump_ok(Storage* storage, const char* path, const char* name, const FileInfo* info) {
    if(name[0] == '\0' || name[0] == '.') return false;
    if(ends_with_i(name, ".bin")) return bin_size_ok(info->size);
    if(ends_with_i(name, ".nfc")) return nfc_header_is_amiibo(storage, path);
    return false;
}

static unsigned count_tags_in(App* app, const char* folder) {
    unsigned count = 0;
    FuriString** subs = NULL;
    uint16_t sub_count = 0;
    uint16_t sub_cap = 0;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, folder)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, (uint16_t)sizeof(name))) {
            if(name[0] == '\0' || name[0] == '.') continue;
            if(info.flags & FSF_DIRECTORY) {
                if(sub_count == sub_cap) {
                    uint16_t next = sub_cap == 0 ? 8 : (uint16_t)(sub_cap * 2);
                    FuriString** grown = realloc(subs, (size_t)next * sizeof(FuriString*));
                    if(!grown) continue;
                    subs = grown;
                    sub_cap = next;
                }
                subs[sub_count++] = furi_string_alloc_printf("%s/%s", folder, name);
            } else if(ends_with_i(name, ".bin") || ends_with_i(name, ".nfc")) {
                FuriString* path = furi_string_alloc_printf("%s/%s", folder, name);
                if(dump_ok(app->storage, furi_string_get_cstr(path), name, &info)) count++;
                furi_string_free(path);
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    for(uint16_t i = 0; i < sub_count; i++) {
        count += count_tags_in(app, furi_string_get_cstr(subs[i]));
        furi_string_free(subs[i]);
    }
    free(subs);
    return count;
}

static void show_scan(App* app) {
    list_reset(app);
    list_header(app, "Scan Amiibos", 0, false);
    list_add(app, "Scan Amiibos", 0, false, MenuRead);
    list_add(
        app, "Scanned Amiibos", count_tags_in(app, APP_DATA_PATH("scanned")), true, MenuUnsaved);
    if(app->my_index != MenuRead && app->my_index != MenuUnsaved) app->my_index = MenuRead;
    list_select(app, app->my_index);
    app->current = ViewMy;
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
}

static void show_settings(App* app) {
    char version[32];
    snprintf(version, sizeof(version), "Version [v%s]", APP_VERSION);
    list_reset(app);
    list_header(app, "Settings", 0, false);
    list_add(app, version, 0, false, MenuVersion);
    list_add(app, app->auto_uid ? "Auto UID: ON" : "Auto UID: OFF", 0, false, MenuAuto);
    list_add(app, app->keys_loaded ? "Keys: Loaded" : "Keys: Missing", 0, false, MenuKeys);
    list_add(
        app,
        app->sort_series ? "Sort lists: By Series" : "Sort lists: Unsorted",
        0,
        false,
        MenuSort);
    list_add(app, "Delete Scanned Amiibos", 0, false, MenuDeleteScanned);
    list_add(app, "Delete Favorites", 0, false, MenuDeleteFavorites);
    if(app->settings_index == 0) app->settings_index = MenuVersion;
    list_select(app, app->settings_index);
    app->current = ViewSettings;
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
}

typedef enum {
    CleanupScanned,
    CleanupFavorites,
} CleanupKind;

static bool path_in_folder(const App* app, const char* folder) {
    const char* path = furi_string_get_cstr(app->path);
    size_t length = strlen(folder);
    if(strncmp(path, folder, length) != 0) return false;
    return path[length] == '/' || path[length] == '\0';
}
static void forget_if_deleted(App* app, bool scanned, bool favorites) {
    bool drop = false;
    if(scanned) {
        drop = drop || path_in_folder(app, APP_DATA_PATH("scanned"));
        drop = drop || path_in_folder(app, APP_DATA_PATH("unsaved"));
    }
    if(favorites) {
        drop = drop || path_in_folder(app, APP_DATA_PATH("favorites"));
        drop = drop || path_in_folder(app, APP_DATA_PATH("saved"));
    }
    if(!drop) return;
    app->loaded = false;
    app->dirty = false;
    app->file_count = 0;
}

static void erase_folder(App* app, const char* folder) {
    storage_simply_remove_recursive(app->storage, folder);
    storage_simply_mkdir(app->storage, folder);
}

static void erase_scanned(App* app) {
    erase_folder(app, APP_DATA_PATH("scanned"));
    storage_simply_remove_recursive(app->storage, APP_DATA_PATH("unsaved"));
    forget_if_deleted(app, true, false);
}

static void erase_favorites(App* app) {
    erase_folder(app, APP_DATA_PATH("favorites"));
    storage_simply_remove_recursive(app->storage, APP_DATA_PATH("saved"));
    forget_if_deleted(app, false, true);
}

typedef struct {
    uint8_t filled;
    uint8_t kind;
} CleanupModel;

static void cleanup_draw(Canvas* canvas, void* context) {
    const CleanupModel* model = context;
    const char* title = "Delete scans?";
    const char* what = "Scanned Amiibos only.";
    if(model->kind == CleanupFavorites) {
        title = "Delete favorites?";
        what = "Favorites only.";
    }
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, what);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Downloaded database");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "on the SD card stays.");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, "Hold Up 2 seconds");
    canvas_draw_frame(canvas, 8, 58, 112, 5);
    if(model->filled > 0) {
        uint8_t width = model->filled > 112 ? 112 : model->filled;
        canvas_draw_box(canvas, 8, 58, width, 5);
    }
}

static void cleanup_reset_bar(App* app) {
    CleanupModel* model = view_get_model(app->cleanup_view);
    model->filled = 0;
    view_commit_model(app->cleanup_view, true);
}

static bool cleanup_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        app->cleanup_holding = false;
        cleanup_reset_bar(app);
        return false;
    }
    if(event->key != InputKeyUp) return true;
    if(event->type == InputTypePress) {
        app->cleanup_holding = true;
        app->cleanup_hold_start = furi_get_tick();
    } else if(event->type == InputTypeRelease) {
        app->cleanup_holding = false;
        cleanup_reset_bar(app);
    }
    return true;
}

static void cleanup_hold_tick(App* app) {
    if(app->current != ViewCleanup) return;
    CleanupModel* model = view_get_model(app->cleanup_view);
    uint8_t filled = 0;
    bool erase = false;
    if(app->cleanup_holding) {
        uint32_t need = furi_ms_to_ticks(2000);
        uint32_t elapsed = furi_get_tick() - app->cleanup_hold_start;
        if(need == 0 || elapsed >= need) {
            app->cleanup_holding = false;
            erase = true;
        } else {
            filled = (uint8_t)(((uint64_t)elapsed * 112) / need);
        }
    }
    bool changed = model->filled != filled;
    model->filled = filled;
    uint8_t kind = model->kind;
    view_commit_model(app->cleanup_view, changed);
    if(!erase) return;
    const char* header = "Deleted";
    const char* text = "Scanned Amiibos\nare gone.\nDownloaded database\nstays.";
    if(kind == CleanupFavorites) {
        erase_favorites(app);
        text = "Favorites\nare gone.\nDownloaded database\nstays.";
    } else {
        erase_scanned(app);
    }
    app_alert(app, header, text);
    show_settings(app);
}

static void show_cleanup(App* app, CleanupKind kind) {
    app->cleanup_holding = false;
    CleanupModel* model = view_get_model(app->cleanup_view);
    model->kind = (uint8_t)kind;
    model->filled = 0;
    view_commit_model(app->cleanup_view, true);
    app->current = ViewCleanup;
    view_dispatcher_switch_to_view(app->dispatcher, ViewCleanup);
}

static bool make_new_uid(App* app) {
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
    return true;
}

static bool assign_uid(App* app) {
    if(!make_new_uid(app)) return false;
    if(app->figure_read) {
        app->figure_read = false;
        app->policy = UidFollowSetting;
    }
    app->dirty = true;
    return true;
}

static void stop_listener(App* app, bool keep_live, bool mark_dirty) {
    if(!app->emulating) return;
    nfc_listener_stop(app->listener);
    if(keep_live) {
        const MfUltralightData* live =
            nfc_listener_get_data(app->listener, NfcProtocolMfUltralight);
        tag_from_pages(app, live);
        nfc_device_set_data(app->device, NfcProtocolMfUltralight, live);
    }
    nfc_listener_free(app->listener);
    app->listener = NULL;
    nfc_free(app->nfc);
    app->nfc = NULL;
    app->emulating = false;
    if(mark_dirty) app->dirty = true;
    notification_message(app->notifications, &sequence_blink_stop);
}

static void stop_emulate(App* app) {
    stop_listener(app, true, true);
}

static bool start_emulate(App* app, bool renew) {
    if(!app->loaded) {
        app_alert(app, "No dump", "Open an Amiibo\nfirst.");
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
    format_uid_display(uid, uid_text, sizeof(uid_text));
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
    if(auto_uid_enabled(app)) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Next ID", widget_callback, app);
    }
    app->current = ViewEmulate;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
    notification_message(app->notifications, &sequence_blink_start_magenta);
    return true;
}

static bool write_dump_at(App* app, const char* path) {
    if(!app->loaded) return false;
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
    bool existed = storage_common_exists(app->storage, path);
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
        if(existed && storage_common_rename_safe(app->storage, path, backup_path) != FSE_OK) break;
        if(storage_common_rename_safe(app->storage, temp_path, path) != FSE_OK) {
            recovery = existed &&
                       storage_common_rename_safe(app->storage, backup_path, path) != FSE_OK;
            break;
        }
        created = false;
        saved = true;
        if(existed && storage_common_remove(app->storage, backup_path) != FSE_OK) {
            app_alert(app, "Saved", "Old dump retained\nas .amiibo.bak");
        }
    } while(false);
    if(created && !recovery) storage_common_remove(app->storage, temp_path);
    if(recovery)
        app_alert(app, "Save interrupted", "Original is in\n.amiibo.bak\nRestore it first.");
    else if(!saved)
        app_alert(app, "Save failed", "Original dump was\nnot replaced.");
    furi_string_free(backup);
    furi_string_free(temp);
    return saved;
}

static bool save_dump(App* app) {
    if(!write_dump_at(app, furi_string_get_cstr(app->path))) return false;
    app->dirty = false;
    return true;
}

static bool load_selected(App* app) {
    const char* path = furi_string_get_cstr(app->path);
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
    app->loaded = true;
    app->dirty = false;
    app->figure_read = false;
    set_policy_from_path(app);
    return true;
}

enum {
    EventReadOk = 200,
    EventReadFail = 201,
    EventMenuTick = 210,
    EventSectionLeft = 220,
    EventSectionRight = 221,
    EventTagPrev = 230,
    EventTagNext = 231,
    EventFavNoted = 232,
    EventBlastStop = 233,
};

#define READ_WAIT_MS 5000

static bool read_wait_elapsed(const App* app) {
    return (int32_t)(furi_get_tick() - app->read_started) >=
           (int32_t)furi_ms_to_ticks(READ_WAIT_MS);
}

static NfcCommand read_keep_trying(App* app) {
    if(!read_wait_elapsed(app)) return NfcCommandReset;
    view_dispatcher_send_custom_event(app->dispatcher, EventReadFail);
    return NfcCommandStop;
}

static void amiibo_password_from_uid(const uint8_t uid[7], uint8_t password[4]) {
    password[0] = (uint8_t)(0xAA ^ uid[1] ^ uid[3]);
    password[1] = (uint8_t)(0x55 ^ uid[2] ^ uid[4]);
    password[2] = (uint8_t)(0xAA ^ uid[3] ^ uid[5]);
    password[3] = (uint8_t)(0x55 ^ uid[4] ^ uid[6]);
}

static NfcCommand read_poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;
    if(event.protocol != NfcProtocolMfUltralight) return NfcCommandContinue;
    const MfUltralightPollerEvent* mf_event = event.event_data;

    if(mf_event->type == MfUltralightPollerEventTypeRequestMode) {
        mf_event->data->poller_mode = MfUltralightPollerModeRead;
        return NfcCommandContinue;
    }

    if(mf_event->type == MfUltralightPollerEventTypeAuthRequest) {
        const MfUltralightData* data = nfc_poller_get_data(app->poller);
        size_t uid_len = 0;
        const uint8_t* uid = NULL;
        if(data && data->iso14443_3a_data) {
            uid = iso14443_3a_get_uid(data->iso14443_3a_data, &uid_len);
        }
        if(uid && uid_len == 7) {
            amiibo_password_from_uid(uid, mf_event->data->auth_context.password.data);
            mf_event->data->auth_context.skip_auth = false;
        } else {
            mf_event->data->auth_context.skip_auth = true;
        }
        return NfcCommandContinue;
    }

    if(mf_event->type == MfUltralightPollerEventTypeReadSuccess) {
        const MfUltralightData* data = nfc_poller_get_data(app->poller);
        if(data && data->type == MfUltralightTypeNTAG215 && data->pages_total == 135 &&
           data->pages_read == 135) {
            memset(app->tag, 0, sizeof(app->tag));
            tag_from_pages(app, data);
            app->bin_size = AMIIBO_TAG_SIZE;
            memset(app->bin_suffix, 0, sizeof(app->bin_suffix));
            nfc_device_set_data(app->device, NfcProtocolMfUltralight, data);
            amiibo_fill_blank_trailer(app->tag);
            amiibo_apply_password(app->tag);
            view_dispatcher_send_custom_event(app->dispatcher, EventReadOk);
            return NfcCommandStop;
        }
        return read_keep_trying(app);
    }

    if(mf_event->type == MfUltralightPollerEventTypeReadFailed ||
       mf_event->type == MfUltralightPollerEventTypeCardLocked ||
       mf_event->type == MfUltralightPollerEventTypeAuthFailed) {
        return read_keep_trying(app);
    }

    return NfcCommandContinue;
}

static void stop_read(App* app) {
    if(!app->reading) return;
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    app->poller = NULL;
    nfc_free(app->nfc);
    app->nfc = NULL;
    app->reading = false;
    notification_message(app->notifications, &sequence_blink_stop);
}

typedef struct {
    uint8_t frame;
} SplashModel;

#define SPLASH_FRAME_MS 120
#define SPLASH_HOLD_MS  2000

#define BLAST_SEND_MS  1000
#define BLAST_PAUSE_MS 2000

static void blast_restore(App* app) {
    memcpy(app->tag, app->blast_saved, AMIIBO_TAG_SIZE);
    if(app->loaded) pages_from_tag(app);
    app->dirty = false;
}

static void blast_show(App* app, bool on_air) {
    uint8_t uid[7];
    char uid_text[24];
    amiibo_uid_from_tag(app->tag, uid);
    format_uid_display(uid, uid_text, sizeof(uid_text));
    FuriString* name = furi_string_alloc();
    path_extract_filename(app->path, name, true);
    furi_string_printf(
        app->info,
        on_air ? "\e#Blasting\n%s\n%s\nAny key stops" : "\e#Blast pause\n%s\n%s\nAny key stops",
        furi_string_get_cstr(name),
        uid_text);
    furi_string_free(name);
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info));
    app->current = ViewEmulate;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
    if(on_air) notification_message(app->notifications, &sequence_blink_start_magenta);
}

static bool blast_go_on_air(App* app) {
    if(!make_new_uid(app)) return false;
    pages_from_tag(app);
    app->nfc = nfc_alloc();
    const MfUltralightData* data = nfc_device_get_data(app->device, NfcProtocolMfUltralight);
    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(app->listener, NULL, NULL);
    app->emulating = true;
    app->blast_on_air = true;
    app->blast_phase_started = furi_get_tick();
    blast_show(app, true);
    return true;
}

static void blast_pause(App* app) {
    stop_listener(app, false, false);
    app->blast_on_air = false;
    app->blast_phase_started = furi_get_tick();
    blast_show(app, false);
}

static void blast_stop(App* app) {
    if(!app->blasting) return;
    app->blasting = false;
    app->blast_on_air = false;
    if(app->emulating) stop_listener(app, false, false);
    blast_restore(app);
    show_tag(app);
}

static void blast_start(App* app) {
    if(!app->loaded) {
        app_alert(app, "No dump", "Open an Amiibo\nfirst.");
        return;
    }
    memcpy(app->blast_saved, app->tag, AMIIBO_TAG_SIZE);
    app->blasting = true;
    if(!blast_go_on_air(app)) {
        app->blasting = false;
        blast_restore(app);
        show_tag(app);
    }
}

static void blast_tick(App* app) {
    if(!app->blasting) return;
    uint32_t phase_ms = app->blast_on_air ? BLAST_SEND_MS : BLAST_PAUSE_MS;
    if((furi_get_tick() - app->blast_phase_started) < furi_ms_to_ticks(phase_ms)) return;
    if(app->blast_on_air) {
        blast_pause(app);
        return;
    }
    if(!blast_go_on_air(app)) {
        app->blasting = false;
        blast_restore(app);
        show_tag(app);
    }
}

static void app_tick(void* context) {
    App* app = context;
    if(app->current == ViewCleanup) cleanup_hold_tick(app);
    if(app->blasting) blast_tick(app);
    if(app->current != ViewSplash) return;
    uint32_t frame_ticks = furi_ms_to_ticks(SPLASH_FRAME_MS);
    uint32_t elapsed = furi_get_tick() - app->splash_started;
    uint32_t done = frame_ticks * SPLASH_FRAME_COUNT + furi_ms_to_ticks(SPLASH_HOLD_MS);
    if(elapsed >= done) {
        show_menu(app);
        return;
    }
    uint32_t frame = elapsed / frame_ticks;
    if(frame >= SPLASH_FRAME_COUNT) frame = SPLASH_FRAME_COUNT - 1;
    SplashModel* model = view_get_model(app->splash_view);
    bool changed = model->frame != (uint8_t)frame;
    model->frame = (uint8_t)frame;
    view_commit_model(app->splash_view, changed);
}

static void splash_draw(Canvas* canvas, void* model) {
    const SplashModel* splash = model;
    uint8_t frame = splash->frame;
    if(frame >= SPLASH_FRAME_COUNT) frame = SPLASH_FRAME_COUNT - 1;
    canvas_clear(canvas);
    canvas_draw_xbm(canvas, 0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, splash_frames[frame]);
}

static bool splash_input(InputEvent* event, void* context) {
    UNUSED(context);
    return event->key != InputKeyBack;
}

static void show_splash(App* app) {
    app->splash_started = furi_get_tick();
    app->current = ViewSplash;
    view_dispatcher_switch_to_view(app->dispatcher, ViewSplash);
}

static void start_read(App* app) {
    if(app->emulating) stop_emulate(app);
    if(app->reading) stop_read(app);

    furi_string_set(
        app->info,
        "\e#Scan Amiibo\nHold your Amiibo to\nthe back of your\nFlipper Zero.\nBack button cancels.");
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info));
    app->current = ViewRead;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);

    app->nfc = nfc_alloc();
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    app->reading = true;
    app->read_started = furi_get_tick();
    nfc_poller_start(app->poller, read_poller_callback, app);
    notification_message(app->notifications, &sequence_blink_start_cyan);
}

static void show_read_failed(App* app) {
    furi_string_set(app->info, "\e#Not read\nFailed after\n5 seconds.\nTry again.");
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 48, furi_string_get_cstr(app->info));
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", widget_callback, app);
    app->current = ViewRead;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
}

static void finish_read(App* app, bool ok) {
    stop_read(app);
    if(!ok) {
        show_read_failed(app);
        return;
    }

    set_read_path(app);
    app->loaded = true;
    app->origin = OriginUnsaved;
    app->figure_read = true;
    app->policy = UidForceOff;
    if(!save_dump(app)) app->dirty = true;
    app->browse = BrowseNone;
    show_scan(app);
}

static int hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Third byte on a Flipper "Page 22:" line. That byte is the figure-id series. */
static bool third_page_byte(const char* text, const char* end, uint8_t* out) {
    int hi = -1;
    int got = 0;
    for(; text < end && *text != '\n' && *text != '\r'; text++) {
        int nibble = hex_nibble(*text);
        if(nibble < 0) {
            if(hi >= 0) return false;
            continue;
        }
        if(hi < 0) {
            hi = nibble;
        } else {
            uint8_t byte = (uint8_t)((hi << 4) | nibble);
            hi = -1;
            if(got == 2) {
                *out = byte;
                return true;
            }
            got++;
        }
    }
    return false;
}

static bool series_from_bin(Storage* storage, const char* path, uint8_t* series) {
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t id[8];
        if(storage_file_seek(file, 0x54, true) &&
           storage_file_read(file, id, sizeof(id)) == sizeof(id)) {
            *series = id[6];
            ok = true;
        }
        storage_file_close(file);
    }
    storage_file_free(file);

    return ok;
}

static bool series_from_nfc(Storage* storage, const char* path, uint8_t* series) {
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char* buf = malloc(8192);
        if(buf) {
            size_t n = storage_file_read(file, buf, 8192);
            static const char mark[] = "Page 22:";
            for(size_t i = 0; i + sizeof(mark) - 1 < n; i++) {
                if(memcmp(buf + i, mark, sizeof(mark) - 1) == 0) {
                    ok = third_page_byte(buf + i + sizeof(mark) - 1, buf + n, series);
                    break;
                }
            }
            free(buf);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

static bool file_series(Storage* storage, const char* path, uint8_t* series) {
    if(ends_with_i(path, ".bin")) return series_from_bin(storage, path, series);
    if(ends_with_i(path, ".nfc")) return series_from_nfc(storage, path, series);
    return false;
}

static void add_file_rows(App* app) {
    if(app->file_count == 0) {
        list_add(app, "None yet", 0, false, MenuEmpty);
        return;
    }
    for(uint8_t i = 0; i < app->file_count; i++) {
        FuriString* label = furi_string_alloc();
        path_extract_filename(app->files[i], label, true);
        list_add(app, furi_string_get_cstr(label), 0, false, MenuFileBase + i);
        furi_string_free(label);
    }
}
static void show_cut_notice(App* app, unsigned shown);

static void show_file_list(App* app, bool saved) {
    if(!app->sort_series) app->in_series = false;
    app->list_cut = false;
    const char* folder = saved ? APP_DATA_PATH("favorites") : APP_DATA_PATH("scanned");
    app->file_count = 0;
    uint16_t counts[256];
    memset(counts, 0, sizeof(counts));
    uint16_t other = 0;
    uint16_t total = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, folder)) {
        FileInfo info;
        char name[128];
        FuriString* path = furi_string_alloc();
        while(storage_dir_read(dir, &info, name, (uint16_t)sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(!ends_with_i(name, ".bin") && !ends_with_i(name, ".nfc")) continue;
            furi_string_printf(path, "%s/%s", folder, name);
            if(!dump_ok(app->storage, furi_string_get_cstr(path), name, &info)) continue;
            if(!app->sort_series) {
                if(app->file_count < FILE_MAX) {
                    furi_string_set(app->files[app->file_count], path);
                    app->file_count++;
                } else {
                    app->list_cut = true;
                    break;
                }
                continue;
            }
            total++;
            uint8_t series = 0;
            bool known = file_series(app->storage, furi_string_get_cstr(path), &series) &&
                         amiibo_series_name(series) != NULL;
            if(known)
                counts[series]++;
            else
                other++;
            if(app->in_series) {
                bool take = app->in_other ? !known : (known && series == app->open_series);
                if(take) {
                    if(app->file_count < FILE_MAX) {
                        furi_string_set(app->files[app->file_count], path);
                        app->file_count++;
                    } else {
                        app->list_cut = true;
                    }
                }
            }
        }
        furi_string_free(path);
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    list_reset(app);
    if(app->sort_series && !app->in_series) {
        const char* root_title = "Scanned Amiibos";
        if(saved && app->blast_list)
            root_title = "Amiibo Blast";
        else if(saved)
            root_title = "Favorites";
        list_header(app, root_title, total, true);
        app->series_shown = 0;
        for(size_t i = 0; i < amiibo_series_count() && app->series_shown < 32; i++) {
            uint8_t id = 0;
            const char* label = amiibo_series_at(i, &id);
            if(!label || counts[id] == 0) continue;
            FuriString* row = furi_string_alloc_printf("%s/", label);
            app->series_ids[app->series_shown] = id;
            app->series_other[app->series_shown] = 0;
            list_add(
                app, furi_string_get_cstr(row), counts[id], true, MenuFileBase + app->series_shown);
            furi_string_free(row);
            app->series_shown++;
        }
        if(other > 0 && app->series_shown < 32) {
            app->series_ids[app->series_shown] = 0;
            app->series_other[app->series_shown] = 1;
            list_add(app, "Other/", other, true, MenuFileBase + app->series_shown);
            app->series_shown++;
        }
        if(app->series_shown == 0) {
            list_add(app, "None yet", 0, false, MenuEmpty);
            list_select(app, MenuEmpty);
        } else {
            list_select(app, app->series_index);
        }
    } else if(app->sort_series) {
        const char* title = "Other";
        uint16_t header_count = other;
        if(!app->in_other) {
            const char* name = amiibo_series_name(app->open_series);
            if(name) title = name;
            header_count = counts[app->open_series];
        }
        list_header(app, title, header_count, true);
        add_file_rows(app);
        uint32_t selected = app->unsaved_index;
        if(saved && app->blast_list)
            selected = app->blast_index;
        else if(saved)
            selected = app->saved_index;
        if(app->file_count == 0) selected = MenuEmpty;
        list_select(app, selected);
    } else {
        const char* root_title = "Scanned Amiibos";
        if(saved && app->blast_list)
            root_title = "Amiibo Blast";
        else if(saved)
            root_title = "Favorites";
        list_header(app, root_title, count_tags_in(app, folder), true);
        add_file_rows(app);
        uint32_t selected = app->unsaved_index;
        if(saved && app->blast_list)
            selected = app->blast_index;
        else if(saved)
            selected = app->saved_index;
        if(app->file_count == 0) selected = MenuEmpty;
        list_select(app, selected);
    }
    if(saved && app->blast_list)
        app->current = ViewBlast;
    else
        app->current = saved ? ViewSaved : ViewUnsaved;
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
    if(app->list_cut) show_cut_notice(app, FILE_MAX);
}

static bool load_listed_file(App* app, uint8_t index, bool saved) {
    if(index >= app->file_count || !furi_string_size(app->files[index])) return false;
    FuriString* previous = furi_string_alloc_set(app->path);
    furi_string_set(app->path, app->files[index]);
    bool loaded = load_selected(app);
    if(!loaded) furi_string_set(app->path, previous);
    furi_string_free(previous);
    if(!loaded) return false;
    app->policy = UidForceOff;
    app->figure_read = true;
    if(saved && app->blast_list) {
        app->origin = OriginBlast;
        app->browse = BrowseBlast;
        app->blast_index = MenuFileBase + index;
    } else if(saved) {
        app->origin = OriginSaved;
        app->browse = BrowseSaved;
        app->saved_index = MenuFileBase + index;
    } else {
        app->origin = OriginUnsaved;
        app->browse = BrowseUnsaved;
        app->unsaved_index = MenuFileBase + index;
    }
    show_tag(app);
    return true;
}

static void open_series_row(App* app, uint8_t index) {
    bool saved = app->current == ViewSaved || app->current == ViewBlast;
    if(index >= app->series_shown) {
        show_file_list(app, saved);
        return;
    }
    app->series_index = MenuFileBase + index;
    app->in_series = true;
    app->in_other = app->series_other[index] != 0;
    app->open_series = app->series_ids[index];
    if(saved && app->blast_list)
        app->blast_index = MenuFileBase;
    else if(saved)
        app->saved_index = MenuFileBase;
    else
        app->unsaved_index = MenuFileBase;
    show_file_list(app, saved);
}

static void open_listed(App* app, uint8_t index) {
    bool saved = app->current == ViewSaved || app->current == ViewBlast;
    if(app->sort_series && !app->in_series) {
        open_series_row(app, index);
        return;
    }
    if(!load_listed_file(app, index, saved)) show_file_list(app, saved);
}

static bool confirm_delete(App* app) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "Delete?", 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(
        message, "This Amiibo\nwill be removed.", 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Cancel", NULL, "Delete");
    DialogMessageButton button = dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
    return button == DialogMessageButtonRight;
}

static void delete_current(App* app) {
    bool saved = app->origin == OriginSaved;
    if(!confirm_delete(app)) {
        show_tag(app);
        return;
    }
    storage_common_remove(app->storage, furi_string_get_cstr(app->path));
    app->loaded = false;
    app->dirty = false;
    show_file_list(app, saved);
}

static bool copy_to_favorites(App* app) {
    FuriString* name = furi_string_alloc();
    FuriString* dest = furi_string_alloc();
    path_extract_filename(app->path, name, false);
    furi_string_printf(dest, "%s/%s", APP_DATA_PATH("favorites"), furi_string_get_cstr(name));
    bool ok = write_dump_at(app, furi_string_get_cstr(dest));
    furi_string_free(name);
    furi_string_free(dest);
    return ok;
}

static const char UID_CONFIRM[] = "Generating a new UID\n"
                                  "(Unique Identifier)\n"
                                  "helps bypass daily\n"
                                  "scan restrictions in\n"
                                  "games like\n"
                                  "The Legend of Zelda.\n"
                                  "Press again to\n"
                                  "confirm you want to\n"
                                  "change the UID for\n"
                                  "this 'Favorite'\n"
                                  "Amiibo";

static void fav_notice_done(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, EventFavNoted);
}

static void show_cut_notice(App* app, unsigned shown) {
    char line[24];
    snprintf(line, sizeof(line), "First %u shown", shown);
    app->notice_return = app->current;
    app->cut_notice = true;
    popup_reset(app->popup);
    popup_set_callback(app->popup, fav_notice_done);
    popup_set_context(app->popup, app);
    popup_set_header(app->popup, "List shortened", 64, 16, AlignCenter, AlignCenter);
    popup_set_text(app->popup, line, 64, 36, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);
    app->current = ViewNotice;
    view_dispatcher_switch_to_view(app->dispatcher, ViewNotice);
}

static void show_fav_notice(App* app) {
    app->cut_notice = false;
    popup_reset(app->popup);
    popup_set_callback(app->popup, fav_notice_done);
    popup_set_context(app->popup, app);
    popup_set_header(app->popup, "Added to Favorites", 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 2000);
    popup_enable_timeout(app->popup);
    app->current = ViewNotice;
    view_dispatcher_switch_to_view(app->dispatcher, ViewNotice);
}

static void show_uid_confirm(App* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 48, UID_CONFIRM);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "New UID", widget_callback, app);
    app->uid_confirm = true;
    app->current = ViewTag;
    view_dispatcher_switch_to_view(app->dispatcher, ViewTag);
}

static bool new_uid_on_favorite(App* app) {
    uint8_t previous[AMIIBO_TAG_SIZE];
    memcpy(previous, app->tag, sizeof(previous));
    if(!assign_uid(app)) return false;
    app->origin = OriginSaved;
    app->policy = UidForceOff;
    app->figure_read = true;
    if(!save_dump(app)) {
        memcpy(app->tag, previous, sizeof(previous));
        pages_from_tag(app);
        app->dirty = false;
        app->origin = OriginSaved;
        app->policy = UidForceOff;
        app->figure_read = true;
        return false;
    }
    return true;
}

static void menu_draw(Canvas* canvas, void* context) {
    const MenuModel* model = context;
    canvas_clear(canvas);
    uint8_t index = model->index % MENU_ICON_COUNT;
    canvas_draw_xbm(
        canvas, -model->offset, 0, MENU_ICON_WIDTH, MENU_ICON_HEIGHT, menu_icons[index]);
    if(model->offset > 0) {
        uint8_t next = (index + 1) % MENU_ICON_COUNT;
        canvas_draw_xbm(
            canvas,
            MENU_ICON_WIDTH - model->offset,
            0,
            MENU_ICON_WIDTH,
            MENU_ICON_HEIGHT,
            menu_icons[next]);
    } else if(model->offset < 0) {
        uint8_t prev = (index + MENU_ICON_COUNT - 1) % MENU_ICON_COUNT;
        canvas_draw_xbm(
            canvas,
            -MENU_ICON_WIDTH - model->offset,
            0,
            MENU_ICON_WIDTH,
            MENU_ICON_HEIGHT,
            menu_icons[prev]);
    }
}

static void carousel_open(App* app) {
    MenuModel* model = view_get_model(app->menu_view);
    uint8_t index = model->index % MENU_ICON_COUNT;
    view_commit_model(app->menu_view, false);
    if(index == 0) {
        show_database(app);
    } else if(index == 1) {
        app->blast_list = false;
        app->in_series = false;
        show_file_list(app, true);
    } else if(index == 2) {
        show_scan(app);
    } else if(index == 3) {
        app->blast_list = true;
        app->in_series = false;
        show_file_list(app, true);
    } else if(index == 4) {
        show_settings(app);
    } else if(index == 5) {
        app->current = ViewHelp;
        view_dispatcher_switch_to_view(app->dispatcher, ViewHelp);
    } else {
        show_about(app);
    }
}

static int section_of(App* app) {
    if(app->current == ViewDatabase) return 0;
    if(app->current == ViewSaved) return 1;
    if(app->current == ViewMy) return 2;
    if(app->current == ViewBlast) return 3;
    if(app->current == ViewSettings) return 4;
    if(app->current == ViewHelp) return 5;
    if(app->current == ViewAbout) return 6;
    return -1;
}

static void show_section(App* app, int index) {
    if(index < 0) index += MENU_ICON_COUNT;
    index %= MENU_ICON_COUNT;
    MenuModel* model = view_get_model(app->menu_view);
    model->index = (uint8_t)index;
    model->offset = 0;
    model->dir = 0;
    view_commit_model(app->menu_view, false);
    furi_timer_stop(app->menu_timer);
    carousel_open(app);
}

static void section_key(const void* message, void* context) {
    const InputEvent* event = message;
    App* app = context;
    if(app->blasting && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->dispatcher, EventBlastStop);
        return;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;
    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        if(event->type != InputTypeShort) return;
        view_dispatcher_send_custom_event(
            app->dispatcher, event->key == InputKeyRight ? EventSectionRight : EventSectionLeft);
        return;
    }
    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        view_dispatcher_send_custom_event(
            app->dispatcher, event->key == InputKeyDown ? EventTagNext : EventTagPrev);
    }
}

static bool menu_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        furi_timer_stop(app->menu_timer);
        MenuModel* model = view_get_model(app->menu_view);
        model->offset = 0;
        model->dir = 0;
        view_commit_model(app->menu_view, false);
        return false;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    MenuModel* model = view_get_model(app->menu_view);
    bool busy = model->dir != 0;
    if(!busy && event->key == InputKeyOk && event->type == InputTypeShort) {
        view_commit_model(app->menu_view, false);
        carousel_open(app);
        return true;
    }
    int8_t dir = 0;
    if(event->key == InputKeyRight)
        dir = 1;
    else if(event->key == InputKeyLeft)
        dir = -1;
    if(!busy && dir != 0) {
        model->dir = dir;
        view_commit_model(app->menu_view, false);
        furi_timer_start(app->menu_timer, furi_ms_to_ticks(4));
        return true;
    }
    view_commit_model(app->menu_view, false);
    return true;
}

static void menu_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, EventMenuTick);
}

static void menu_tick(App* app) {
    MenuModel* model = view_get_model(app->menu_view);
    if(model->dir == 0) {
        view_commit_model(app->menu_view, false);
        return;
    }
    model->offset += (int16_t)(model->dir * 16);
    if(model->offset >= MENU_ICON_WIDTH || model->offset <= -MENU_ICON_WIDTH) {
        if(model->dir > 0)
            model->index = (model->index + 1) % MENU_ICON_COUNT;
        else
            model->index = (model->index + MENU_ICON_COUNT - 1) % MENU_ICON_COUNT;
        model->offset = 0;
        model->dir = 0;
        furi_timer_stop(app->menu_timer);
    }
    view_commit_model(app->menu_view, true);
}

static int db_name_cmp(const char* left, const char* right) {
    while(*left && *right) {
        unsigned char a = (unsigned char)*left;
        unsigned char b = (unsigned char)*right;
        if(a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if(b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if(a != b) return (int)a - (int)b;
        left++;
        right++;
    }
    return (int)(unsigned char)*left - (int)(unsigned char)*right;
}

static void db_clear(App* app) {
    for(uint16_t i = 0; i < app->db_count; i++)
        furi_string_free(app->db_names[i]);
    free(app->db_names);
    free(app->db_is_dir);
    app->db_names = NULL;
    app->db_is_dir = NULL;
    app->db_count = 0;
    app->db_capacity = 0;
}

static bool db_push(App* app, const char* name, bool is_dir) {
    if(app->db_count == app->db_capacity) {
        uint16_t next = app->db_capacity == 0 ? 32 : (uint16_t)(app->db_capacity * 2);
        if(next < app->db_capacity) return false;
        FuriString** names = realloc(app->db_names, (size_t)next * sizeof(FuriString*));
        uint8_t* dirs = realloc(app->db_is_dir, next);
        if(!names || !dirs) {
            if(names) app->db_names = names;
            if(dirs) app->db_is_dir = dirs;
            return false;
        }
        app->db_names = names;
        app->db_is_dir = dirs;
        app->db_capacity = next;
    }
    app->db_names[app->db_count] = furi_string_alloc_set(name);
    app->db_is_dir[app->db_count] = is_dir ? 1 : 0;
    app->db_count++;
    return true;
}

static void db_sort(App* app) {
    for(uint16_t i = 1; i < app->db_count; i++) {
        FuriString* name = app->db_names[i];
        uint8_t is_dir = app->db_is_dir[i];
        uint16_t j = i;
        while(j > 0) {
            uint8_t prev_dir = app->db_is_dir[j - 1];
            bool earlier = is_dir != prev_dir ?
                               is_dir > prev_dir :
                               db_name_cmp(
                                   furi_string_get_cstr(name),
                                   furi_string_get_cstr(app->db_names[j - 1])) < 0;
            if(!earlier) break;
            app->db_names[j] = app->db_names[j - 1];
            app->db_is_dir[j] = prev_dir;
            j--;
        }
        app->db_names[j] = name;
        app->db_is_dir[j] = is_dir;
    }
}

static bool db_is_root(const App* app) {
    return furi_string_cmp_str(app->db_path, APP_DATA_PATH("db")) == 0;
}

static void db_load(App* app) {
    db_clear(app);
    app->list_cut = false;
    File* dir = storage_file_alloc(app->storage);
    bool opened = storage_dir_open(dir, furi_string_get_cstr(app->db_path));
    if(!opened && !db_is_root(app)) {
        storage_file_free(dir);
        furi_string_set(app->db_path, APP_DATA_PATH("db"));
        app->db_depth = 0;
        db_load(app);
        return;
    }
    if(opened) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, (uint16_t)sizeof(name))) {
            if(name[0] == '\0' || name[0] == '.') continue;
            bool is_dir = (info.flags & FSF_DIRECTORY) != 0;
            if(!is_dir) {
                if(!ends_with_i(name, ".bin") && !ends_with_i(name, ".nfc")) continue;
                FuriString* path =
                    furi_string_alloc_printf("%s/%s", furi_string_get_cstr(app->db_path), name);
                bool ok = dump_ok(app->storage, furi_string_get_cstr(path), name, &info);
                furi_string_free(path);
                if(!ok) continue;
            }
            if(app->db_count >= DIR_LIST_MAX || !db_push(app, name, is_dir)) {
                app->list_cut = true;
                break;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    db_sort(app);
}

static void show_database(App* app) {
    db_load(app);
    FuriString* title = furi_string_alloc();
    if(db_is_root(app)) {
        furi_string_set(title, "Amiibo Database");
    } else {
        path_extract_filename(app->db_path, title, false);
    }
    unsigned title_count = count_tags_in(app, furi_string_get_cstr(app->db_path));
    list_reset(app);
    list_header(app, furi_string_get_cstr(title), title_count, true);
    furi_string_free(title);
    if(app->db_count == 0) {
        list_add(app, "None yet", 0, false, MenuEmpty);
    } else {
        if(app->db_index >= app->db_count) app->db_index = 0;
        for(uint16_t i = 0; i < app->db_count; i++) {
            FuriString* label = furi_string_alloc_set(app->db_names[i]);
            unsigned row_count = 0;
            bool folder = app->db_is_dir[i] != 0;
            if(folder) {
                furi_string_cat(label, "/");
                FuriString* child = furi_string_alloc_printf(
                    "%s/%s",
                    furi_string_get_cstr(app->db_path),
                    furi_string_get_cstr(app->db_names[i]));
                row_count = count_tags_in(app, furi_string_get_cstr(child));
                furi_string_free(child);
            } else {
                size_t dot = furi_string_search_rchar(label, '.');
                if(dot != FURI_STRING_FAILURE) furi_string_left(label, dot);
            }
            list_add(app, furi_string_get_cstr(label), row_count, folder, MenuFileBase + i);
            furi_string_free(label);
        }
    }
    list_select(app, app->db_count == 0 ? MenuEmpty : MenuFileBase + app->db_index);
    app->current = ViewDatabase;
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
    if(app->list_cut) show_cut_notice(app, DIR_LIST_MAX);
}

static void db_up(App* app) {
    if(db_is_root(app) || app->db_depth == 0) {
        show_menu(app);
        return;
    }
    size_t slash = furi_string_search_rchar(app->db_path, '/');
    if(slash != FURI_STRING_FAILURE) furi_string_left(app->db_path, slash);
    app->db_depth--;
    app->db_index = app->db_stack[app->db_depth];
    show_database(app);
}

static void open_db_item(App* app, uint16_t index) {
    if(index >= app->db_count) {
        show_database(app);
        return;
    }
    app->db_index = index;
    if(app->db_is_dir[index]) {
        if(app->db_depth < 8) app->db_stack[app->db_depth++] = index;
        furi_string_cat_printf(app->db_path, "/%s", furi_string_get_cstr(app->db_names[index]));
        app->db_index = 0;
        show_database(app);
        return;
    }
    FuriString* previous = furi_string_alloc_set(app->path);
    furi_string_printf(
        app->path,
        "%s/%s",
        furi_string_get_cstr(app->db_path),
        furi_string_get_cstr(app->db_names[index]));
    bool loaded = load_selected(app);
    if(!loaded) furi_string_set(app->path, previous);
    furi_string_free(previous);
    if(!loaded) {
        show_database(app);
        return;
    }
    app->origin = OriginDatabase;
    app->browse = BrowseDatabase;
    show_tag(app);
}

static int db_file_neighbor(App* app, int dir) {
    if(app->db_count == 0) return -1;
    int start = (int)app->db_index;
    if(start < 0 || start >= (int)app->db_count) start = 0;
    int index = start;
    for(uint16_t step = 0; step < app->db_count; step++) {
        index += dir;
        if(index < 0)
            index = (int)app->db_count - 1;
        else if(index >= (int)app->db_count)
            index = 0;
        if(index == start) return start;
        if(!app->db_is_dir[index]) return index;
    }
    return start;
}

static void
    restore_open_tag(App* app, const char* path, const uint8_t tag[AMIIBO_TAG_SIZE], bool loaded) {
    furi_string_set(app->path, path);
    memcpy(app->tag, tag, AMIIBO_TAG_SIZE);
    app->loaded = loaded;
    if(loaded) pages_from_tag(app);
    show_tag(app);
}

static void step_tag(App* app, int dir) {
    if(dir == 0 || app->browse == BrowseNone) return;

    FuriString* previous = furi_string_alloc_set(app->path);
    uint8_t tag_copy[AMIIBO_TAG_SIZE];
    memcpy(tag_copy, app->tag, sizeof(tag_copy));
    bool loaded = app->loaded;
    bool ok = true;

    if(app->browse == BrowseDatabase) {
        int next = db_file_neighbor(app, dir);
        if(next >= 0 && next != (int)app->db_index && !app->db_is_dir[next]) {
            furi_string_printf(
                app->path,
                "%s/%s",
                furi_string_get_cstr(app->db_path),
                furi_string_get_cstr(app->db_names[next]));
            ok = load_selected(app);
            if(ok) {
                app->db_index = (uint32_t)next;
                app->origin = OriginDatabase;
                app->browse = BrowseDatabase;
                show_tag(app);
            }
        }
    } else if(app->file_count > 1) {
        bool blast = app->browse == BrowseBlast;
        bool saved = blast || app->browse == BrowseSaved;
        uint32_t selected = blast ? app->blast_index :
                            saved ? app->saved_index :
                                    app->unsaved_index;
        int pos = 0;
        if(selected >= MenuFileBase && selected < MenuFileBase + (uint32_t)app->file_count) {
            pos = (int)(selected - MenuFileBase);
        }
        int next = pos + dir;
        if(next < 0)
            next = app->file_count - 1;
        else if(next >= app->file_count)
            next = 0;
        if(next != pos) {
            furi_string_set(app->path, app->files[next]);
            ok = load_selected(app);
            if(ok) {
                app->policy = UidForceOff;
                app->figure_read = true;
                if(blast) {
                    app->origin = OriginBlast;
                    app->browse = BrowseBlast;
                    app->blast_index = MenuFileBase + (uint32_t)next;
                } else if(saved) {
                    app->origin = OriginSaved;
                    app->browse = BrowseSaved;
                    app->saved_index = MenuFileBase + (uint32_t)next;
                } else {
                    app->origin = OriginUnsaved;
                    app->browse = BrowseUnsaved;
                    app->unsaved_index = MenuFileBase + (uint32_t)next;
                }
                show_tag(app);
            }
        }
    }

    if(!ok) restore_open_tag(app, furi_string_get_cstr(previous), tag_copy, loaded);
    furi_string_free(previous);
}

static void widget_callback(GuiButtonType button, InputType type, void* context) {
    App* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->dispatcher, button + 100);
}

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    if(app->current == ViewSettings)
        app->settings_index = index;
    else if(app->current == ViewMy)
        app->my_index = index;
    else if(app->current == ViewSaved)
        app->saved_index = index;
    else if(app->current == ViewBlast)
        app->blast_index = index;
    else if(app->current == ViewUnsaved)
        app->unsaved_index = index;
    else if(app->current == ViewDatabase && index >= MenuFileBase)
        app->db_index = index - MenuFileBase;
    view_dispatcher_send_custom_event(app->dispatcher, index);
}

static bool custom_event(void* context, uint32_t event) {
    App* app = context;
    if(event == EventMenuTick) {
        menu_tick(app);
        return true;
    }
    if(event == EventBlastStop) {
        blast_stop(app);
        return true;
    }
    if(event == EventSectionLeft || event == EventSectionRight) {
        int section = section_of(app);
        if(!app->reading && !app->emulating && section >= 0) {
            show_section(app, section + (event == EventSectionRight ? 1 : -1));
        }
        return true;
    }
    if(event == EventFavNoted) {
        if(app->cut_notice) {
            app->cut_notice = false;
            app->current = app->notice_return;
            view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
        } else if(app->current == ViewNotice) {
            show_tag(app);
        }
        return true;
    }
    if(event == EventTagPrev || event == EventTagNext) {
        if(app->current == ViewTag && !app->uid_confirm && !app->reading && !app->emulating) {
            step_tag(app, event == EventTagNext ? 1 : -1);
        }
        return true;
    }
    if(app->reading && (event == EventReadOk || event == EventReadFail)) {
        finish_read(app, event == EventReadOk);
        return true;
    }
    if(app->reading) return true;

    if(app->current == ViewRead && event == GuiButtonTypeCenter + 100) {
        start_read(app);
        return true;
    }

    if(app->emulating && event == GuiButtonTypeCenter + 100) {
        if(app->blasting) {
            blast_stop(app);
            return true;
        }
        stop_emulate(app);
        if(!start_emulate(app, true)) show_tag(app);
        return true;
    }
    if(app->emulating) return true;

    if(app->current == ViewDatabase && event >= MenuFileBase &&
       event < (uint32_t)MenuFileBase + app->db_count) {
        open_db_item(app, (uint16_t)(event - MenuFileBase));
        return true;
    }
    if(event >= MenuFileBase && event < MenuFileBase + FILE_MAX) {
        open_listed(app, (uint8_t)(event - MenuFileBase));
        return true;
    }
    if(event == MenuEmpty) {
        if(app->current == ViewDatabase)
            db_up(app);
        else if(
            app->sort_series && app->in_series &&
            (app->current == ViewSaved || app->current == ViewBlast ||
             app->current == ViewUnsaved)) {
            app->in_series = false;
            show_file_list(app, app->current != ViewUnsaved);
        } else if(app->current == ViewSaved || app->current == ViewBlast)
            show_menu(app);
        else
            show_scan(app);
    } else if(event == MenuUnsaved) {
        app->in_series = false;
        show_file_list(app, false);
    } else if(event == MenuRead) {
        start_read(app);
    } else if(event == MenuVersion) {
        show_settings(app);
    } else if(event == MenuAuto) {
        app->auto_uid = !app->auto_uid;
        save_settings(app);
        show_settings(app);
    } else if(event == MenuSort) {
        app->sort_series = !app->sort_series;
        app->in_series = false;
        save_settings(app);
        show_settings(app);
    } else if(event == MenuKeys) {
        if(app->keys_loaded) {
            app_alert(app, "Keys loaded", app->key_path);
        } else {
            app_alert(app, "Keys missing", "Put key_retail.bin\nin apps_data/\namiibo_emulator");
        }
        show_settings(app);
    } else if(event == MenuDeleteScanned) {
        show_cleanup(app, CleanupScanned);
    } else if(event == MenuDeleteFavorites) {
        show_cleanup(app, CleanupFavorites);
    } else if(event == GuiButtonTypeLeft + 100) {
        if(app->uid_confirm) {
            app->uid_confirm = false;
            new_uid_on_favorite(app);
            show_tag(app);
        } else if(app->origin == OriginUnsaved || app->origin == OriginDatabase) {
            if(copy_to_favorites(app)) show_fav_notice(app);
        } else if(app->origin == OriginSaved) {
            show_uid_confirm(app);
        }
    } else if(event == GuiButtonTypeCenter + 100) {
        if(app->origin == OriginBlast)
            blast_start(app);
        else
            start_emulate(app, false);
    } else if(event == GuiButtonTypeRight + 100) {
        if(app->origin == OriginDatabase) {
            if(assign_uid(app)) show_tag(app);
        } else if(app->origin != OriginBlast) {
            delete_current(app);
        }
    }
    return true;
}

static bool back_event(void* context) {
    App* app = context;
    if(app->reading || app->current == ViewRead) {
        stop_read(app);
        show_scan(app);
        return true;
    }
    if(app->blasting) {
        blast_stop(app);
        return true;
    }
    if(app->emulating || app->current == ViewEmulate) {
        stop_emulate(app);
        if(app->origin == OriginSaved || app->origin == OriginUnsaved) save_dump(app);
        show_tag(app);
        return true;
    }
    if(app->current == ViewCleanup) {
        show_settings(app);
        return true;
    }
    if(app->current == ViewDatabase) {
        db_up(app);
        return true;
    }
    if(app->current == ViewSaved || app->current == ViewBlast) {
        if(app->sort_series && app->in_series) {
            app->in_series = false;
            show_file_list(app, true);
        } else {
            show_menu(app);
        }
        return true;
    }
    if(app->current == ViewUnsaved) {
        if(app->sort_series && app->in_series) {
            app->in_series = false;
            show_file_list(app, false);
        } else {
            show_scan(app);
        }
        return true;
    }
    if(app->current == ViewMy || app->current == ViewSettings || app->current == ViewHelp ||
       app->current == ViewAbout) {
        show_menu(app);
        return true;
    }
    if(app->current == ViewNotice) {
        if(app->cut_notice) {
            app->cut_notice = false;
            app->current = app->notice_return;
            view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
        } else {
            show_tag(app);
        }
        return true;
    }
    if(app->uid_confirm) {
        show_tag(app);
        return true;
    }
    if(app->current == ViewTag) {
        if(app->origin == OriginSaved || app->origin == OriginBlast)
            show_file_list(app, true);
        else if(app->origin == OriginUnsaved)
            show_file_list(app, false);
        else
            show_database(app);
        return true;
    }
    return false;
}

static void move_legacy_files(Storage* storage, const char* from, const char* to) {
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, from)) {
        storage_file_free(dir);
        return;
    }
    FileInfo info;
    char name[128];
    FuriString* src = furi_string_alloc();
    FuriString* dst = furi_string_alloc();
    while(storage_dir_read(dir, &info, name, (uint16_t)sizeof(name))) {
        if(info.flags & FSF_DIRECTORY) continue;
        furi_string_printf(src, "%s/%s", from, name);
        furi_string_printf(dst, "%s/%s", to, name);
        storage_common_rename(storage, furi_string_get_cstr(src), furi_string_get_cstr(dst));
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_string_free(src);
    furi_string_free(dst);
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
    storage_simply_mkdir(app->storage, APP_DATA_PATH(""));
    storage_simply_mkdir(app->storage, APP_DATA_PATH("db"));
    storage_simply_mkdir(app->storage, APP_DATA_PATH("favorites"));
    storage_simply_mkdir(app->storage, APP_DATA_PATH("scanned"));
    move_legacy_files(app->storage, APP_DATA_PATH("saved"), APP_DATA_PATH("favorites"));
    move_legacy_files(app->storage, APP_DATA_PATH("unsaved"), APP_DATA_PATH("scanned"));
    load_settings(app);
    load_keys(app);

    app->path = furi_string_alloc_set(APP_DATA_PATH("db"));
    app->db_path = furi_string_alloc_set(APP_DATA_PATH("db"));
    for(uint8_t i = 0; i < FILE_MAX; i++)
        app->files[i] = furi_string_alloc();
    app->info = furi_string_alloc();
    app->device = nfc_device_alloc();
    app->dispatcher = view_dispatcher_alloc();
    app->list_view = view_alloc();
    view_allocate_model(app->list_view, ViewModelTypeLocking, sizeof(ListModel));
    with_view_model(
        app->list_view, ListModel * model, { model->header = furi_string_alloc(); }, false);
    view_set_context(app->list_view, app);
    view_set_draw_callback(app->list_view, list_draw);
    view_set_input_callback(app->list_view, list_input);
    app->widget = widget_alloc();
    app->tag_buttons = view_alloc();
    view_allocate_model(app->tag_buttons, ViewModelTypeLocking, sizeof(TagBarModel));
    TagBarModel* tag_bar = view_get_model(app->tag_buttons);
    tag_bar->app = app;
    view_commit_model(app->tag_buttons, false);
    view_set_context(app->tag_buttons, app);
    view_set_draw_callback(app->tag_buttons, tag_buttons_draw);
    view_set_input_callback(app->tag_buttons, tag_buttons_input);
    app->tag_stack = view_stack_alloc();
    view_stack_add_view(app->tag_stack, widget_get_view(app->widget));
    view_stack_add_view(app->tag_stack, app->tag_buttons);
    app->splash_view = view_alloc();
    view_allocate_model(app->splash_view, ViewModelTypeLocking, sizeof(SplashModel));
    SplashModel* splash_model = view_get_model(app->splash_view);
    splash_model->frame = 0;
    view_commit_model(app->splash_view, false);
    view_set_context(app->splash_view, app);
    view_set_draw_callback(app->splash_view, splash_draw);
    view_set_input_callback(app->splash_view, splash_input);
    app->cleanup_view = view_alloc();
    view_allocate_model(app->cleanup_view, ViewModelTypeLocking, sizeof(CleanupModel));
    view_set_context(app->cleanup_view, app);
    view_set_draw_callback(app->cleanup_view, cleanup_draw);
    view_set_input_callback(app->cleanup_view, cleanup_input);
    app->menu_view = view_alloc();
    view_allocate_model(app->menu_view, ViewModelTypeLocking, sizeof(MenuModel));
    view_set_context(app->menu_view, app);
    view_set_draw_callback(app->menu_view, menu_draw);
    view_set_input_callback(app->menu_view, menu_input);
    app->popup = popup_alloc();
    app->menu_timer = furi_timer_alloc(menu_timer_cb, FuriTimerTypePeriodic, app);
    app->help_view = view_alloc();
    view_allocate_model(app->help_view, ViewModelTypeLocking, sizeof(PageModel));
    PageModel* help_model = view_get_model(app->help_view);
    help_model->scroll = 0;
    help_model->title = "Help";
    help_model->body = HELP;
    help_model->lines = page_line_count(HELP);
    help_model->visible = 4;
    view_commit_model(app->help_view, false);
    view_set_context(app->help_view, app);
    view_set_draw_callback(app->help_view, page_draw);
    view_set_input_callback(app->help_view, help_input);
    app->about_view = view_alloc();
    view_allocate_model(app->about_view, ViewModelTypeLocking, sizeof(PageModel));
    PageModel* about_model = view_get_model(app->about_view);
    about_model->scroll = 0;
    about_model->title = "About";
    about_model->body = ABOUT;
    about_model->lines = page_line_count(ABOUT);
    about_model->visible = 4;
    view_commit_model(app->about_view, false);
    view_set_context(app->about_view, app);
    view_set_draw_callback(app->about_view, page_draw);
    view_set_input_callback(app->about_view, about_input);
    app->input_pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    app->input_subscription = furi_pubsub_subscribe(app->input_pubsub, section_key, app);

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, custom_event);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back_event);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_add_view(app->dispatcher, ViewSplash, app->splash_view);
    view_dispatcher_add_view(app->dispatcher, ViewCleanup, app->cleanup_view);
    view_dispatcher_add_view(app->dispatcher, ViewCarousel, app->menu_view);
    view_dispatcher_add_view(app->dispatcher, ViewMenu, app->list_view);
    view_dispatcher_add_view(app->dispatcher, ViewTag, view_stack_get_view(app->tag_stack));
    view_dispatcher_add_view(app->dispatcher, ViewNotice, popup_get_view(app->popup));
    view_dispatcher_add_view(app->dispatcher, ViewHelp, app->help_view);
    view_dispatcher_add_view(app->dispatcher, ViewAbout, app->about_view);

    view_dispatcher_set_tick_event_callback(app->dispatcher, app_tick, furi_ms_to_ticks(50));
    show_splash(app);
    view_dispatcher_run(app->dispatcher);

    if(app->reading) stop_read(app);
    if(app->emulating) stop_emulate(app);
    furi_timer_stop(app->menu_timer);
    furi_pubsub_unsubscribe(app->input_pubsub, app->input_subscription);
    furi_record_close(RECORD_INPUT_EVENTS);

    view_dispatcher_remove_view(app->dispatcher, ViewSplash);
    view_dispatcher_remove_view(app->dispatcher, ViewCleanup);
    view_dispatcher_remove_view(app->dispatcher, ViewCarousel);
    view_dispatcher_remove_view(app->dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->dispatcher, ViewTag);
    view_stack_remove_view(app->tag_stack, app->tag_buttons);
    view_stack_remove_view(app->tag_stack, widget_get_view(app->widget));
    view_stack_free(app->tag_stack);
    view_free(app->tag_buttons);
    view_dispatcher_remove_view(app->dispatcher, ViewNotice);
    view_dispatcher_remove_view(app->dispatcher, ViewHelp);
    view_dispatcher_remove_view(app->dispatcher, ViewAbout);
    furi_timer_free(app->menu_timer);
    view_free(app->splash_view);
    view_free(app->cleanup_view);
    view_free(app->menu_view);
    view_free(app->help_view);
    view_free(app->about_view);
    widget_free(app->widget);
    popup_free(app->popup);
    with_view_model(
        app->list_view,
        ListModel * model,
        {
            for(uint16_t i = 0; i < model->count; i++)
                furi_string_free(model->items[i].label);
            free(model->items);
            furi_string_free(model->header);
        },
        false);
    view_free(app->list_view);
    view_dispatcher_free(app->dispatcher);
    nfc_device_free(app->device);
    furi_string_free(app->info);
    db_clear(app);
    furi_string_free(app->db_path);
    furi_string_free(app->path);
    for(uint8_t i = 0; i < FILE_MAX; i++)
        furi_string_free(app->files[i]);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
