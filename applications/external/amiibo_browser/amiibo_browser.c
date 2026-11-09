#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <stdlib.h>
#include <string.h>

#define ROOT_PATH    EXT_PATH("nfc/Amiibo/By_Game")
#define MAX_ENTRIES  400
#define MAX_NAME_LEN 64
#define MAX_DEPTH    16
#define ABOUT_INDEX  (UINT32_MAX - 1)

typedef enum {
    ViewMenu,
    ViewEmulate,
    ViewAbout,
} AppView;

typedef struct {
    char name[MAX_NAME_LEN];
    bool is_dir;
} Entry;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* submenu;
    Widget* widget;
    View* about_view;
    int32_t about_scroll;
    Storage* storage;
    NotificationApp* notifications;

    FuriString* path;
    Entry* entries;
    size_t entry_count;
    uint32_t selected;
    uint32_t selected_stack[MAX_DEPTH];
    size_t depth;

    Nfc* nfc;
    NfcDevice* device;
    NfcListener* listener;
    AppView current;
} App;

static void build_menu(App* app);

static int entry_cmp(const void* a, const void* b) {
    const Entry* ea = a;
    const Entry* eb = b;
    if(ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
}

static void display_name(const char* src, char* dst, size_t len, bool strip_ext) {
    strlcpy(dst, src, len);
    if(strip_ext) {
        char* dot = strrchr(dst, '.');
        if(dot) *dot = '\0';
    }
    for(char* p = dst; *p; p++) {
        if(*p == '_') *p = ' ';
    }
}

static void load_entries(App* app) {
    app->entry_count = 0;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, furi_string_get_cstr(app->path))) {
        FileInfo info;
        char name[MAX_NAME_LEN];
        while(app->entry_count < MAX_ENTRIES && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            bool is_dir = file_info_is_dir(&info);
            if(!is_dir) {
                size_t n = strlen(name);
                if(n < 5 || strcasecmp(name + n - 4, ".nfc") != 0) continue;
            }
            strlcpy(app->entries[app->entry_count].name, name, MAX_NAME_LEN);
            app->entries[app->entry_count].is_dir = is_dir;
            app->entry_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    for(size_t i = 1; i < app->entry_count; i++) {
        Entry tmp = app->entries[i];
        size_t j = i;
        while(j > 0 && entry_cmp(&app->entries[j - 1], &tmp) > 0) {
            app->entries[j] = app->entries[j - 1];
            j--;
        }
        app->entries[j] = tmp;
    }
}

static void stop_emulation(App* app) {
    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
    notification_message(app->notifications, &sequence_blink_stop);
}

static NfcCommand listener_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return NfcCommandContinue;
}

static void start_emulation(App* app, const Entry* entry) {
    FuriString* file =
        furi_string_alloc_printf("%s/%s", furi_string_get_cstr(app->path), entry->name);
    char label[MAX_NAME_LEN];
    display_name(entry->name, label, sizeof(label), true);

    widget_reset(app->widget);
    bool ok = nfc_device_load(app->device, furi_string_get_cstr(file)) &&
              nfc_device_get_protocol(app->device) == NfcProtocolMfUltralight;

    if(ok) {
        app->listener = nfc_listener_alloc(
            app->nfc,
            NfcProtocolMfUltralight,
            nfc_device_get_data(app->device, NfcProtocolMfUltralight));
        nfc_listener_start(app->listener, listener_callback, app);
        notification_message(app->notifications, &sequence_blink_start_magenta);

        widget_add_string_element(
            app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Emulating amiibo");
        widget_add_string_multiline_element(
            app->widget, 64, 26, AlignCenter, AlignTop, FontSecondary, label);
        widget_add_string_element(
            app->widget,
            64,
            62,
            AlignCenter,
            AlignBottom,
            FontSecondary,
            "Hold to reader | Back=stop");
    } else {
        widget_add_string_element(
            app->widget, 64, 14, AlignCenter, AlignTop, FontPrimary, "Load failed");
        widget_add_string_multiline_element(
            app->widget, 64, 32, AlignCenter, AlignTop, FontSecondary, label);
    }

    furi_string_free(file);
    app->current = ViewEmulate;
    view_dispatcher_switch_to_view(app->dispatcher, ViewEmulate);
}

#define ABOUT_HEIGHT 92
#define ABOUT_STEP   8

static void draw_triforce(Canvas* c, int cx, int top) {
    // Three solid triangles (side 10px) forming the Triforce, apex at (cx, top).
    for(int r = 0; r < 6; r++) {
        canvas_draw_line(c, cx - r, top + r, cx + r, top + r);
        canvas_draw_line(c, cx - 6 - r, top + 6 + r, cx - 6 + r, top + 6 + r);
        canvas_draw_line(c, cx + 6 - r, top + 6 + r, cx + 6 + r, top + 6 + r);
    }
}

static void about_draw(Canvas* c, void* model) {
    int32_t* scroll = model;
    int y = -*scroll;
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, y + 3, AlignCenter, AlignTop, "Amiibo Browser");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, y + 16, AlignCenter, AlignTop, "by AzureSky");
    static const char* const lines[] = {
        "Amiibo is a trademark of",
        "Nintendo. Characters are",
        "(c) Nintendo. All rights",
        "reserved.",
    };
    for(size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        canvas_draw_str_aligned(c, 64, y + 30 + i * 10, AlignCenter, AlignTop, lines[i]);
    }
    draw_triforce(c, 64, y + 74);
    // scrollbar
    int track = 64 - 4;
    int thumb = track * 64 / ABOUT_HEIGHT;
    int pos = (track - thumb) * *scroll / (ABOUT_HEIGHT - 64);
    canvas_draw_line(c, 126, 2, 126, 61);
    canvas_draw_box(c, 125, 2 + pos, 3, thumb);
}

static bool about_input(InputEvent* e, void* context) {
    App* app = context;
    if(e->type != InputTypePress && e->type != InputTypeRepeat) return false;
    if(e->key == InputKeyDown) {
        if(app->about_scroll < ABOUT_HEIGHT - 64) app->about_scroll += ABOUT_STEP;
    } else if(e->key == InputKeyUp) {
        if(app->about_scroll > 0) app->about_scroll -= ABOUT_STEP;
    } else {
        return false;
    }
    with_view_model(app->about_view, int32_t * m, { *m = app->about_scroll; }, true);
    return true;
}

static void show_about(App* app) {
    app->about_scroll = 0;
    with_view_model(app->about_view, int32_t * m, { *m = 0; }, true);
    app->current = ViewAbout;
    view_dispatcher_switch_to_view(app->dispatcher, ViewAbout);
}

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    if(index == ABOUT_INDEX) {
        app->selected = app->entry_count;
        show_about(app);
        return;
    }
    if(index >= app->entry_count) return;
    app->selected = index;
    Entry* entry = &app->entries[index];
    if(entry->is_dir) {
        if(app->depth >= MAX_DEPTH) return;
        app->selected_stack[app->depth++] = index;
        furi_string_cat_printf(app->path, "/%s", entry->name);
        app->selected = 0;
        build_menu(app);
    } else {
        start_emulation(app, entry);
    }
}

static void build_menu(App* app) {
    load_entries(app);
    submenu_reset(app->submenu);

    const char* p = furi_string_get_cstr(app->path);
    const char* slash = strrchr(p, '/');
    char header[MAX_NAME_LEN];
    bool at_root = furi_string_cmp_str(app->path, ROOT_PATH) == 0;
    if(at_root) {
        strlcpy(header, "Amiibo - Games", sizeof(header));
    } else {
        display_name(slash ? slash + 1 : p, header, sizeof(header), false);
    }
    submenu_set_header(app->submenu, header);

    if(app->entry_count == 0) {
        submenu_add_item(app->submenu, "(empty / missing folder)", UINT32_MAX, NULL, NULL);
    }
    for(size_t i = 0; i < app->entry_count; i++) {
        char label[MAX_NAME_LEN + 4];
        char name[MAX_NAME_LEN];
        display_name(app->entries[i].name, name, sizeof(name), !app->entries[i].is_dir);
        snprintf(label, sizeof(label), app->entries[i].is_dir ? "[%s]" : "%s", name);
        submenu_add_item(app->submenu, label, i, submenu_callback, app);
    }
    if(at_root) {
        submenu_add_item(app->submenu, "About", ABOUT_INDEX, submenu_callback, app);
    }
    submenu_set_selected_item(app->submenu, app->selected);

    app->current = ViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
}

static bool navigation_callback(void* context) {
    App* app = context;
    if(app->current == ViewEmulate) {
        stop_emulation(app);
        build_menu(app);
        return true;
    }
    if(app->current == ViewAbout) {
        build_menu(app);
        return true;
    }
    if(furi_string_cmp_str(app->path, ROOT_PATH) == 0) {
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    size_t pos = furi_string_search_rchar(app->path, '/');
    if(pos != FURI_STRING_FAILURE) furi_string_left(app->path, pos);
    app->selected = app->depth > 0 ? app->selected_stack[--app->depth] : 0;
    build_menu(app);
    return true;
}

int32_t amiibo_browser_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->entries = malloc(sizeof(Entry) * MAX_ENTRIES);
    app->path = furi_string_alloc_set(ROOT_PATH);
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->nfc = nfc_alloc();
    app->device = nfc_device_alloc();

    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, navigation_callback);

    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->about_view = view_alloc();
    view_allocate_model(app->about_view, ViewModelTypeLocking, sizeof(int32_t));
    view_set_context(app->about_view, app);
    view_set_draw_callback(app->about_view, about_draw);
    view_set_input_callback(app->about_view, about_input);
    view_dispatcher_add_view(app->dispatcher, ViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->dispatcher, ViewEmulate, widget_get_view(app->widget));
    view_dispatcher_add_view(app->dispatcher, ViewAbout, app->about_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    build_menu(app);
    view_dispatcher_run(app->dispatcher);

    stop_emulation(app);
    view_dispatcher_remove_view(app->dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->dispatcher, ViewEmulate);
    view_dispatcher_remove_view(app->dispatcher, ViewAbout);
    submenu_free(app->submenu);
    widget_free(app->widget);
    view_free(app->about_view);
    view_dispatcher_free(app->dispatcher);
    nfc_device_free(app->device);
    nfc_free(app->nfc);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    furi_string_free(app->path);
    free(app->entries);
    free(app);
    return 0;
}
