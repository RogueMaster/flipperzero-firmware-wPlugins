#include "menu.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>

#include <new>
#include <string.h>
#include <stdio.h>

namespace flipcraft {
namespace menu {

namespace {

constexpr const char* DATA_DIR = STORAGE_APP_DATA_PATH_PREFIX;     // "/data"
constexpr const char* ASSETS_DIR = STORAGE_APP_ASSETS_PATH_PREFIX; // "/assets"

constexpr int MAX_ITEMS = 32;
constexpr int NAME_LEN = 64;

// Submenu indices reserved for the two action rows below the world list.
constexpr uint32_t IDX_CREATE = 0xF000;
constexpr uint32_t IDX_DELETE = 0xF001;

enum ViewId : uint32_t {
    VIEW_MAIN = 0,
    VIEW_PICKER,
    VIEW_NAME,
    VIEW_CONFIRM,
};

enum PickerMode { PICK_TEMPLATE, PICK_DELETE };

struct MenuApp {
    Gui* gui = nullptr;
    Storage* storage = nullptr;

    ViewDispatcher* vd = nullptr;
    Submenu* main_menu = nullptr;
    Submenu* picker = nullptr;
    TextInput* text_input = nullptr;
    DialogEx* dialog = nullptr;

    uint32_t current = VIEW_MAIN;
    PickerMode picker_mode = PICK_TEMPLATE;

    bool launch = false;
    char result_path[256] = {0};

    char chosen_template[256] = {0};
    char name_buf[NAME_LEN] = {0};

    char delete_path[256] = {0};
    char delete_name[NAME_LEN] = {0};

    char saves[MAX_ITEMS][NAME_LEN] = {{0}};
    int saves_count = 0;
    char templates[MAX_ITEMS][NAME_LEN] = {{0}};
    int templates_count = 0;
};

// Forward declarations for callbacks referenced before their definitions.
void main_callback(void* context, uint32_t index);
void picker_callback(void* context, uint32_t index);

void join_path(char* dst, size_t size, const char* dir, const char* name) {
    snprintf(dst, size, "%s/%s", dir, name);
}

// Copy a file name into `dst` without its ".fcw" extension.
void strip_ext(char* dst, size_t size, const char* name) {
    snprintf(dst, size, "%s", name);
    size_t len = strlen(dst);
    if(len > 4 && strcmp(dst + len - 4, ".fcw") == 0) dst[len - 4] = '\0';
}

// List *.fcw files in `dir` into `names`. Returns the count found.
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

void switch_view(MenuApp* app, uint32_t id) {
    app->current = id;
    view_dispatcher_switch_to_view(app->vd, id);
}

void rebuild_main(MenuApp* app) {
    app->saves_count = scan_dir(app->storage, DATA_DIR, app->saves, MAX_ITEMS);

    Submenu* m = app->main_menu;
    submenu_reset(m);
    submenu_set_header(m, "Flipcraft");
    for(int i = 0; i < app->saves_count; i++)
        submenu_add_item(m, app->saves[i], (uint32_t)i, main_callback, app);
    submenu_add_item(m, "Create", IDX_CREATE, main_callback, app);
    if(app->saves_count > 0)
        submenu_add_item(m, "Delete", IDX_DELETE, main_callback, app);
}

void open_templates(MenuApp* app) {
    app->templates_count = scan_dir(app->storage, ASSETS_DIR, app->templates, MAX_ITEMS);
    app->picker_mode = PICK_TEMPLATE;

    Submenu* p = app->picker;
    submenu_reset(p);
    submenu_set_header(p, "World template");
    for(int i = 0; i < app->templates_count; i++)
        submenu_add_item(p, app->templates[i], (uint32_t)i, picker_callback, app);
    switch_view(app, VIEW_PICKER);
}

void open_delete(MenuApp* app) {
    app->picker_mode = PICK_DELETE;

    Submenu* p = app->picker;
    submenu_reset(p);
    submenu_set_header(p, "Delete world");
    for(int i = 0; i < app->saves_count; i++)
        submenu_add_item(p, app->saves[i], (uint32_t)i, picker_callback, app);
    switch_view(app, VIEW_PICKER);
}

void main_callback(void* context, uint32_t index) {
    MenuApp* app = static_cast<MenuApp*>(context);
    if(index == IDX_CREATE) {
        open_templates(app);
        return;
    }
    if(index == IDX_DELETE) {
        open_delete(app);
        return;
    }
    if((int)index < app->saves_count) {
        join_path(app->result_path, sizeof(app->result_path), DATA_DIR, app->saves[index]);
        app->launch = true;
        view_dispatcher_stop(app->vd);
    }
}

void name_callback(void* context) {
    MenuApp* app = static_cast<MenuApp*>(context);

    char fname[NAME_LEN + 8];
    snprintf(fname, sizeof(fname), "%s.fcw", app->name_buf);
    join_path(app->result_path, sizeof(app->result_path), DATA_DIR, fname);

    if(copy_file(app->storage, app->chosen_template, app->result_path)) {
        app->launch = true;
        view_dispatcher_stop(app->vd);
    } else {
        // No fallback world: if the template copy fails, just return to the list.
        rebuild_main(app);
        switch_view(app, VIEW_MAIN);
    }
}

void dialog_callback(DialogExResult result, void* context) {
    MenuApp* app = static_cast<MenuApp*>(context);
    if(result == DialogExResultRight) storage_simply_remove(app->storage, app->delete_path);
    rebuild_main(app);
    switch_view(app, VIEW_MAIN);
}

void picker_callback(void* context, uint32_t index) {
    MenuApp* app = static_cast<MenuApp*>(context);

    if(app->picker_mode == PICK_TEMPLATE) {
        if((int)index >= app->templates_count) return;
        join_path(
            app->chosen_template, sizeof(app->chosen_template), ASSETS_DIR, app->templates[index]);
        strip_ext(app->name_buf, sizeof(app->name_buf), app->templates[index]);

        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "World name");
        text_input_set_minimum_length(app->text_input, 1);
        text_input_set_result_callback(
            app->text_input, name_callback, app, app->name_buf, sizeof(app->name_buf), true);
        switch_view(app, VIEW_NAME);
    } else {
        if((int)index >= app->saves_count) return;
        join_path(app->delete_path, sizeof(app->delete_path), DATA_DIR, app->saves[index]);
        strncpy(app->delete_name, app->saves[index], NAME_LEN - 1);
        app->delete_name[NAME_LEN - 1] = '\0';

        dialog_ex_reset(app->dialog);
        dialog_ex_set_context(app->dialog, app);
        dialog_ex_set_result_callback(app->dialog, dialog_callback);
        dialog_ex_set_header(app->dialog, "Delete?", 64, 10, AlignCenter, AlignCenter);
        dialog_ex_set_text(app->dialog, app->delete_name, 64, 32, AlignCenter, AlignCenter);
        dialog_ex_set_left_button_text(app->dialog, "No");
        dialog_ex_set_right_button_text(app->dialog, "Yes");
        switch_view(app, VIEW_CONFIRM);
    }
}

bool nav_callback(void* context) {
    MenuApp* app = static_cast<MenuApp*>(context);
    if(app->current == VIEW_MAIN) {
        app->launch = false;
        view_dispatcher_stop(app->vd);
        return true;
    }
    switch_view(app, VIEW_MAIN);
    return true;
}

} // namespace

Result run(Gui* gui, Storage* storage) {
    // Make sure the saves directory exists so Create has somewhere to write.
    storage_common_mkdir(storage, DATA_DIR);

    MenuApp* app = new(std::nothrow) MenuApp();
    Result result;
    if(!app) return result;

    app->gui = gui;
    app->storage = storage;

    app->vd = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->picker = submenu_alloc();
    app->text_input = text_input_alloc();
    app->dialog = dialog_ex_alloc();

    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, nav_callback);

    view_dispatcher_add_view(app->vd, VIEW_MAIN, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->vd, VIEW_PICKER, submenu_get_view(app->picker));
    view_dispatcher_add_view(app->vd, VIEW_NAME, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->vd, VIEW_CONFIRM, dialog_ex_get_view(app->dialog));

    rebuild_main(app);
    switch_view(app, VIEW_MAIN);

    view_dispatcher_attach_to_gui(app->vd, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, VIEW_MAIN);
    view_dispatcher_remove_view(app->vd, VIEW_PICKER);
    view_dispatcher_remove_view(app->vd, VIEW_NAME);
    view_dispatcher_remove_view(app->vd, VIEW_CONFIRM);
    view_dispatcher_free(app->vd);
    submenu_free(app->main_menu);
    submenu_free(app->picker);
    text_input_free(app->text_input);
    dialog_ex_free(app->dialog);

    result.launch = app->launch;
    strncpy(result.path, app->result_path, sizeof(result.path) - 1);
    delete app;
    return result;
}

}
}
