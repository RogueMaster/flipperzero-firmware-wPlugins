#include "menu.h"
#include "../plugin_api.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <flipper_application/flipper_application.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/dialog_ex.h>

#include <new>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace flipcraft {
namespace menu {

namespace {

constexpr const char* DATA_DIR = STORAGE_APP_DATA_PATH_PREFIX; // "/data"
constexpr const char* ASSETS_DIR = STORAGE_APP_ASSETS_PATH_PREFIX; // "/assets"
constexpr const char* ABOUT_PATH = APP_ASSETS_PATH("about.txt");

constexpr int MAX_ITEMS = 32;
constexpr int NAME_LEN = 64;

// Menu tree:
//   Main: Worlds / About
//   Worlds: Create / <saves>
//   Create: Generate / <templates from assets>
//   <save>: Play / Info / Rename / Delete
// Creating always asks for a world name: template -> name; generate -> seed,
// name, size.

constexpr uint32_t IDX_WORLDS = 0;
constexpr uint32_t IDX_ABOUT = 1;
constexpr uint32_t IDX_CREATE = 0xF000;
constexpr uint32_t IDX_GENERATE = 0xE000;

constexpr uint32_t ACT_PLAY = 0, ACT_INFO = 1, ACT_RENAME = 2, ACT_DELETE = 3;

constexpr uint8_t SIZE_CHUNKS[4] = {16, 32, 64, 128};
static const char* const SIZE_LABELS[4] = {"128 x 128", "256 x 256", "512 x 512", "1024 x 1024"};

enum ViewId : uint32_t {
    VIEW_MAIN = 0,
    VIEW_LIST, // worlds / create / world actions / size
    VIEW_TEXT, // seed, world name or rename keyboard
    VIEW_ABOUT, // scrollable text: about.txt or world info
    VIEW_CONFIRM, // delete dialog
};

enum ListMode {
    LIST_WORLDS,
    LIST_CREATE,
    LIST_ACTIONS,
    LIST_SIZE
};
enum TextMode {
    TEXT_SEED,
    TEXT_GEN_NAME,
    TEXT_TPL_NAME,
    TEXT_RENAME
};

struct MenuApp {
    Gui* gui = nullptr;
    Storage* storage = nullptr;

    ViewDispatcher* vd = nullptr;
    Submenu* main_menu = nullptr;
    Submenu* list = nullptr;
    TextInput* text_input = nullptr;
    TextBox* text_box = nullptr;
    DialogEx* dialog = nullptr;

    uint32_t current = VIEW_MAIN;
    ListMode list_mode = LIST_WORLDS;
    TextMode text_mode = TEXT_SEED;
    bool about_from_actions = false; // Back target for the text box

    Action result_action = Action::Quit;
    uint8_t gen_chunks = 16;
    uint32_t gen_seed = 0;
    char result_path[256] = {0};

    char text_buf[NAME_LEN] = {0};
    char chosen_template[256] = {0};
    char sel_name[NAME_LEN] = {0}; // save selected in the Worlds list
    char info_text[512] = {0}; // shared text box content (info / about)

    char saves[MAX_ITEMS][NAME_LEN] = {{0}};
    int saves_count = 0;
    char templates[MAX_ITEMS][NAME_LEN] = {{0}};
    int templates_count = 0;
};

void main_callback(void* context, uint32_t index);
void list_callback(void* context, uint32_t index);
void open_worlds(MenuApp* app);

void join_path(char* dst, size_t size, const char* dir, const char* name) {
    snprintf(dst, size, "%s/%s", dir, name);
}

void strip_ext(char* dst, size_t size, const char* name) {
    snprintf(dst, size, "%s", name);
    size_t len = strlen(dst);
    if(len > 4 && strcmp(dst + len - 4, ".fcw") == 0) dst[len - 4] = '\0';
}

int scan_dir(Storage* storage, const char* dir, char names[][NAME_LEN], int max) {
    File* f = storage_file_alloc(storage);
    int n = 0;
    if(storage_dir_open(f, dir)) {
        FileInfo info;
        char nm[NAME_LEN];
        while(n < max && storage_dir_read(f, &info, nm, sizeof(nm))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(nm);
            if(len > 4 && strcmp(nm + len - 4, ".fcw") == 0) {
                strncpy(names[n], nm, NAME_LEN - 1);
                names[n][NAME_LEN - 1] = '\0';
                n++;
            }
        }
    }
    storage_dir_close(f);
    storage_file_free(f);
    return n;
}

bool copy_file(Storage* storage, const char* src, const char* dst) {
    File* in = storage_file_alloc(storage);
    File* out = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(in, src, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_open(out, dst, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = true;
        uint8_t buf[512];
        while(true) {
            size_t r = storage_file_read(in, buf, sizeof(buf));
            if(r > 0 && storage_file_write(out, buf, r) != r) {
                ok = false;
                break;
            }
            if(r < sizeof(buf)) break; // short read -> EOF
        }
    }
    storage_file_close(in);
    storage_file_close(out);
    storage_file_free(in);
    storage_file_free(out);
    return ok;
}

// A seed is whatever the player typed: a decimal number that fits u32 is used
// as-is, anything else is FNV-1a hashed (so word seeds work).
uint32_t parse_seed(const char* s) {
    size_t len = strlen(s);
    bool digits = len > 0 && len <= 10;
    for(size_t i = 0; i < len && digits; i++)
        digits = s[i] >= '0' && s[i] <= '9';
    if(digits) {
        unsigned long long v = strtoull(s, nullptr, 10);
        if(v <= 0xFFFFFFFFull) return (uint32_t)v;
    }
    uint32_t h = 2166136261u;
    for(size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return h;
}

void switch_view(MenuApp* app, uint32_t id) {
    app->current = id;
    view_dispatcher_switch_to_view(app->vd, id);
}

void open_text(MenuApp* app, TextMode mode, const char* header, void (*cb)(void*)) {
    app->text_mode = mode;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input, cb, app, app->text_buf, sizeof(app->text_buf), true);
    switch_view(app, VIEW_TEXT);
}

void open_text_box(MenuApp* app, bool from_actions) {
    app->about_from_actions = from_actions;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->info_text);
    switch_view(app, VIEW_ABOUT);
}

void open_main(MenuApp* app) {
    Submenu* m = app->main_menu;
    submenu_reset(m);
    submenu_set_header(m, "Flipcraft");
    submenu_add_item(m, "Worlds", IDX_WORLDS, main_callback, app);
    submenu_add_item(m, "About", IDX_ABOUT, main_callback, app);
    switch_view(app, VIEW_MAIN);
}

void open_worlds(MenuApp* app) {
    app->saves_count = scan_dir(app->storage, DATA_DIR, app->saves, MAX_ITEMS);
    app->list_mode = LIST_WORLDS;

    Submenu* m = app->list;
    submenu_reset(m);
    submenu_set_header(m, "Worlds");
    submenu_add_item(m, "Create", IDX_CREATE, list_callback, app);
    for(int i = 0; i < app->saves_count; i++)
        submenu_add_item(m, app->saves[i], (uint32_t)i, list_callback, app);
    switch_view(app, VIEW_LIST);
}

void open_create(MenuApp* app) {
    app->templates_count = scan_dir(app->storage, ASSETS_DIR, app->templates, MAX_ITEMS);
    app->list_mode = LIST_CREATE;

    Submenu* m = app->list;
    submenu_reset(m);
    submenu_set_header(m, "New world");
    submenu_add_item(m, "Generate", IDX_GENERATE, list_callback, app);
    for(int i = 0; i < app->templates_count; i++)
        submenu_add_item(m, app->templates[i], (uint32_t)i, list_callback, app);
    switch_view(app, VIEW_LIST);
}

void open_actions(MenuApp* app) {
    app->list_mode = LIST_ACTIONS;

    Submenu* m = app->list;
    submenu_reset(m);
    submenu_set_header(m, app->sel_name);
    submenu_add_item(m, "Play", ACT_PLAY, list_callback, app);
    submenu_add_item(m, "Info", ACT_INFO, list_callback, app);
    submenu_add_item(m, "Rename", ACT_RENAME, list_callback, app);
    submenu_add_item(m, "Delete", ACT_DELETE, list_callback, app);
    switch_view(app, VIEW_LIST);
}

void open_size(MenuApp* app) {
    app->list_mode = LIST_SIZE;

    Submenu* m = app->list;
    submenu_reset(m);
    submenu_set_header(m, "World size");
    for(uint32_t i = 0; i < 4; i++)
        submenu_add_item(m, SIZE_LABELS[i], i, list_callback, app);
    switch_view(app, VIEW_LIST);
}

void open_about(MenuApp* app) {
    File* f = storage_file_alloc(app->storage);
    size_t n = 0;
    if(storage_file_open(f, ABOUT_PATH, FSAM_READ, FSOM_OPEN_EXISTING))
        n = storage_file_read(f, app->info_text, sizeof(app->info_text) - 1);
    storage_file_close(f);
    storage_file_free(f);
    if(n == 0) snprintf(app->info_text, sizeof(app->info_text), "about.txt missing");
    app->info_text[n ? n : strlen(app->info_text)] = '\0';
    open_text_box(app, false);
}

void open_info(MenuApp* app) {
    char path[256];
    join_path(path, sizeof(path), DATA_DIR, app->sel_name);

    File* f = storage_file_alloc(app->storage);
    uint8_t hdr[64];
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_read(f, hdr, sizeof(hdr)) == sizeof(hdr);
    uint32_t bytes = ok ? storage_file_size(f) : 0;
    storage_file_close(f);
    storage_file_free(f);

    auto u16 = [&](int o) { return (uint32_t)hdr[o] | ((uint32_t)hdr[o + 1] << 8); };
    auto u32 = [&](int o) { return u16(o) | (u16(o + 2) << 16); };

    if(ok && u32(0) == 0x31574346) {
        uint32_t cx = u16(6), cz = u16(8);
        snprintf(
            app->info_text,
            sizeof(app->info_text),
            "%s\n"
            "World: %lu x %lu blocks\n"
            "Format: v%lu, %lu KB\n"
            "Seed/rng: %lu\n"
            "Player: %ld, %ld, %ld",
            app->sel_name,
            (unsigned long)(cx * 8),
            (unsigned long)(cz * 8),
            (unsigned long)u16(4),
            (unsigned long)(bytes / 1024),
            (unsigned long)u32(32),
            (long)((int32_t)u32(18) / 16),
            (long)((int32_t)u32(22) / 16),
            (long)((int32_t)u32(26) / 16));
    } else {
        snprintf(
            app->info_text, sizeof(app->info_text), "%s\nNot a Flipcraft world", app->sel_name);
    }
    open_text_box(app, true);
}

void main_callback(void* context, uint32_t index) {
    MenuApp* app = static_cast<MenuApp*>(context);
    if(index == IDX_WORLDS)
        open_worlds(app);
    else if(index == IDX_ABOUT)
        open_about(app);
}

void build_result_path(MenuApp* app) {
    char fname[NAME_LEN + 8];
    snprintf(fname, sizeof(fname), "%s.fcw", app->text_buf);
    join_path(app->result_path, sizeof(app->result_path), DATA_DIR, fname);
}

void text_callback(void* context) {
    MenuApp* app = static_cast<MenuApp*>(context);
    switch(app->text_mode) {
    case TEXT_SEED:
        // Seed accepted; text_buf keeps it as the suggested world name.
        app->gen_seed = parse_seed(app->text_buf);
        open_text(app, TEXT_GEN_NAME, "World name", text_callback);
        break;
    case TEXT_GEN_NAME:
        build_result_path(app);
        open_size(app);
        break;
    case TEXT_TPL_NAME:
        build_result_path(app);
        if(copy_file(app->storage, app->chosen_template, app->result_path)) {
            app->result_action = Action::Launch;
            view_dispatcher_stop(app->vd);
        } else {
            open_worlds(app); // copy failed: back to the list
        }
        break;
    case TEXT_RENAME: {
        char oldp[256], newp[256];
        join_path(oldp, sizeof(oldp), DATA_DIR, app->sel_name);
        char fname[NAME_LEN + 8];
        snprintf(fname, sizeof(fname), "%s.fcw", app->text_buf);
        join_path(newp, sizeof(newp), DATA_DIR, fname);
        if(strcmp(oldp, newp) != 0) storage_common_rename(app->storage, oldp, newp);
        open_worlds(app);
        break;
    }
    }
}

void dialog_callback(DialogExResult result, void* context) {
    MenuApp* app = static_cast<MenuApp*>(context);
    if(result == DialogExResultRight) {
        char path[256];
        join_path(path, sizeof(path), DATA_DIR, app->sel_name);
        storage_simply_remove(app->storage, path);
    }
    open_worlds(app);
}

void list_callback(void* context, uint32_t index) {
    MenuApp* app = static_cast<MenuApp*>(context);

    switch(app->list_mode) {
    case LIST_WORLDS:
        if(index == IDX_CREATE) {
            open_create(app);
        } else if((int)index < app->saves_count) {
            strncpy(app->sel_name, app->saves[index], NAME_LEN - 1);
            app->sel_name[NAME_LEN - 1] = '\0';
            open_actions(app);
        }
        break;

    case LIST_CREATE:
        if(index == IDX_GENERATE) {
            snprintf(
                app->text_buf, sizeof(app->text_buf), "%lu", (unsigned long)furi_hal_random_get());
            open_text(app, TEXT_SEED, "World seed", text_callback);
        } else if((int)index < app->templates_count) {
            join_path(
                app->chosen_template,
                sizeof(app->chosen_template),
                ASSETS_DIR,
                app->templates[index]);
            strip_ext(app->text_buf, sizeof(app->text_buf), app->templates[index]);
            open_text(app, TEXT_TPL_NAME, "World name", text_callback);
        }
        break;

    case LIST_ACTIONS:
        switch(index) {
        case ACT_PLAY:
            join_path(app->result_path, sizeof(app->result_path), DATA_DIR, app->sel_name);
            app->result_action = Action::Launch;
            view_dispatcher_stop(app->vd);
            break;
        case ACT_INFO:
            open_info(app);
            break;
        case ACT_RENAME:
            strip_ext(app->text_buf, sizeof(app->text_buf), app->sel_name);
            open_text(app, TEXT_RENAME, "New name", text_callback);
            break;
        case ACT_DELETE:
            dialog_ex_reset(app->dialog);
            dialog_ex_set_context(app->dialog, app);
            dialog_ex_set_result_callback(app->dialog, dialog_callback);
            dialog_ex_set_header(app->dialog, "Delete?", 64, 10, AlignCenter, AlignCenter);
            dialog_ex_set_text(app->dialog, app->sel_name, 64, 32, AlignCenter, AlignCenter);
            dialog_ex_set_left_button_text(app->dialog, "No");
            dialog_ex_set_right_button_text(app->dialog, "Yes");
            switch_view(app, VIEW_CONFIRM);
            break;
        }
        break;

    case LIST_SIZE:
        if(index < 4) {
            app->gen_chunks = SIZE_CHUNKS[index];
            app->result_action = Action::Generate;
            view_dispatcher_stop(app->vd);
        }
        break;
    }
}

bool nav_callback(void* context) {
    MenuApp* app = static_cast<MenuApp*>(context);
    switch(app->current) {
    case VIEW_MAIN:
        app->result_action = Action::Quit;
        view_dispatcher_stop(app->vd);
        break;
    case VIEW_LIST:
        if(app->list_mode == LIST_WORLDS)
            open_main(app);
        else if(app->list_mode == LIST_ACTIONS || app->list_mode == LIST_CREATE)
            open_worlds(app);
        else
            open_create(app); // size list backs out to Create
        break;
    case VIEW_ABOUT:
        if(app->about_from_actions)
            open_actions(app);
        else
            open_main(app);
        break;
    default: // keyboards, delete dialog
        if(app->list_mode == LIST_ACTIONS)
            open_actions(app);
        else
            open_worlds(app);
        break;
    }
    return true;
}

} // namespace

Result run(Gui* gui, Storage* storage) {
    storage_common_mkdir(storage, DATA_DIR);

    MenuApp* app = new(std::nothrow) MenuApp();
    Result result;
    if(!app) return result;

    app->gui = gui;
    app->storage = storage;

    app->vd = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->list = submenu_alloc();
    app->text_input = text_input_alloc();
    app->text_box = text_box_alloc();
    app->dialog = dialog_ex_alloc();

    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, nav_callback);

    view_dispatcher_add_view(app->vd, VIEW_MAIN, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->vd, VIEW_LIST, submenu_get_view(app->list));
    view_dispatcher_add_view(app->vd, VIEW_TEXT, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->vd, VIEW_ABOUT, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->vd, VIEW_CONFIRM, dialog_ex_get_view(app->dialog));

    open_main(app);

    view_dispatcher_attach_to_gui(app->vd, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, VIEW_MAIN);
    view_dispatcher_remove_view(app->vd, VIEW_LIST);
    view_dispatcher_remove_view(app->vd, VIEW_TEXT);
    view_dispatcher_remove_view(app->vd, VIEW_ABOUT);
    view_dispatcher_remove_view(app->vd, VIEW_CONFIRM);
    view_dispatcher_free(app->vd);
    submenu_free(app->main_menu);
    submenu_free(app->list);
    text_input_free(app->text_input);
    text_box_free(app->text_box);
    dialog_ex_free(app->dialog);

    result.action = app->result_action;
    result.chunks = app->gen_chunks;
    result.seed = app->gen_seed;
    strncpy(result.path, app->result_path, sizeof(result.path) - 1);
    delete app;
    return result;
}

}
}

static FlipcraftMenuAction
    flipcraft_menu_run(char* out_path, size_t out_size, uint8_t* out_chunks, uint32_t* out_seed) {
    Gui* gui = reinterpret_cast<Gui*>(furi_record_open(RECORD_GUI));
    Storage* storage = reinterpret_cast<Storage*>(furi_record_open(RECORD_STORAGE));

    flipcraft::menu::Result result = flipcraft::menu::run(gui, storage);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    if(result.action != flipcraft::menu::Action::Quit && out_path && out_size) {
        strncpy(out_path, result.path, out_size - 1);
        out_path[out_size - 1] = '\0';
    }
    if(out_chunks) *out_chunks = result.chunks;
    if(out_seed) *out_seed = result.seed;
    switch(result.action) {
    case flipcraft::menu::Action::Launch:
        return FlipcraftMenuActionLaunch;
    case flipcraft::menu::Action::Generate:
        return FlipcraftMenuActionGenerate;
    default:
        return FlipcraftMenuActionQuit;
    }
}

static const FlipcraftMenuApi flipcraft_menu_api = {
    .run = flipcraft_menu_run,
};

static const FlipperAppPluginDescriptor flipcraft_menu_descriptor = {
    .appid = FLIPCRAFT_MENU_APP_ID,
    .ep_api_version = FLIPCRAFT_MENU_API_VERSION,
    .entry_point = &flipcraft_menu_api,
};

extern "C" const FlipperAppPluginDescriptor* flipcraft_menu_ep(void) {
    return &flipcraft_menu_descriptor;
}
