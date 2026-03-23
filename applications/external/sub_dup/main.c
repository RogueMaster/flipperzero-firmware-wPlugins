#include "logic.h"
#include "version.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <string.h>

// Forward declaration if header is missing
uint32_t furi_hal_crc_calc_crc32(uint32_t crc, const uint8_t* data, size_t size);

#define BUFFER_SIZE   256
#define FULL_PATH_LEN 280

typedef enum {
    SubDupFinderViewSubmenu,
    SubDupFinderViewGroups,
    SubDupFinderViewFiles,
    SubDupFinderViewConfirm,
    SubDupFinderViewCredits,
    SubDupFinderViewPopup,
} SubDupFinderView;

typedef enum {
    SubDupFinderSubmenuIndexScan,
    SubDupFinderSubmenuIndexCredits,
} SubDupFinderSubmenuIndex;

struct {
    ViewDispatcher* view_dispatcher;
    Submenu* main_submenu;
    Submenu* groups_submenu;
    Submenu* files_in_group_submenu;
    DialogEx* confirm_dialog;
    Widget* credits_widget;
    Popup* popup;
    HashDatabase db;
    char selected_path[APP_MAX_PATH_LEN];
    size_t selected_group_index;
    bool is_scanning;
} app_state;

// Forward declarations for rendering
void render_groups_submenu();
void render_files_in_group(size_t group_index);

uint32_t compute_file_hash(Storage* storage, const char* path) {
    uint32_t hash = 0;
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buffer[BUFFER_SIZE];
        size_t read;
        while((read = storage_file_read(file, buffer, BUFFER_SIZE)) > 0) {
            hash = calculate_crc32(hash, buffer, read);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    return hash;
}

void delete_file_and_update_db(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, path);
    furi_record_close(RECORD_STORAGE);

    for(size_t i = 0; i < app_state.db.count; i++) {
        char full_path[FULL_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "/ext/subghz/%s", app_state.db.records[i].path);
        if(strcmp(full_path, path) == 0) {
            for(size_t j = i; j < app_state.db.count - 1; j++) {
                app_state.db.records[j] = app_state.db.records[j + 1];
            }
            app_state.db.count--;
            break;
        }
    }
}

void confirm_callback(DialogExResult result, void* context) {
    UNUSED(context);
    if(result == DialogExResultRight) {
        uint32_t current_hash = app_state.db.groups[app_state.selected_group_index].hash;

        delete_file_and_update_db(app_state.selected_path);

        process_duplicates(&app_state.db);
        render_groups_submenu();

        int new_index = -1;
        for(size_t i = 0; i < app_state.db.num_groups; i++) {
            if(app_state.db.groups[i].hash == current_hash) {
                new_index = (int)i;
                break;
            }
        }

        if(new_index == -1) {
            view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewGroups);
        } else {
            app_state.selected_group_index = (size_t)new_index;
            render_files_in_group(app_state.selected_group_index);
            view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewFiles);
        }
    } else {
        view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewFiles);
    }
}

uint32_t back_to_submenu(void* context) {
    UNUSED(context);
    return SubDupFinderViewSubmenu;
}

uint32_t exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

uint32_t back_to_groups(void* context) {
    UNUSED(context);
    return SubDupFinderViewGroups;
}

uint32_t back_to_files(void* context) {
    UNUSED(context);
    return SubDupFinderViewFiles;
}

void files_in_group_callback(void* context, uint32_t index) {
    UNUSED(context);
    const DuplicateGroup* group = &app_state.db.groups[app_state.selected_group_index];
    size_t record_index = group->start_index + index;

    strncpy(
        app_state.selected_path,
        app_state.db.records[record_index].path,
        sizeof(app_state.selected_path) - 1);
    char full_path[FULL_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "/ext/subghz/%s", app_state.selected_path);
    strncpy(app_state.selected_path, full_path, sizeof(app_state.selected_path) - 1);

    dialog_ex_set_header(app_state.confirm_dialog, "Delete file?", 64, 10, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app_state.confirm_dialog, app_state.selected_path, 64, 30, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewConfirm);
}

void render_files_in_group(size_t group_index) {
    submenu_reset(app_state.files_in_group_submenu);
    view_set_previous_callback(submenu_get_view(app_state.files_in_group_submenu), back_to_groups);

    const DuplicateGroup* group = &app_state.db.groups[group_index];
    for(size_t i = 0; i < group->count; i++) {
        submenu_add_item(
            app_state.files_in_group_submenu,
            app_state.db.records[group->start_index + i].path,
            i,
            files_in_group_callback,
            NULL);
    }
}

void groups_submenu_callback(void* context, uint32_t index) {
    UNUSED(context);
    app_state.selected_group_index = index;
    render_files_in_group(index);
    view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewFiles);
}

void render_groups_submenu() {
    submenu_reset(app_state.groups_submenu);
    view_set_previous_callback(submenu_get_view(app_state.groups_submenu), back_to_submenu);

    for(size_t i = 0; i < app_state.db.num_groups; i++) {
        char group_name[64];
        snprintf(
            group_name,
            sizeof(group_name),
            "Dup Group %zu (%zu)",
            i + 1,
            app_state.db.groups[i].count);
        submenu_add_item(app_state.groups_submenu, group_name, i, groups_submenu_callback, NULL);
    }
}

static int32_t scan_thread_callback(void* context) {
    UNUSED(context);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    app_state.db.count = 0;

    if(storage_dir_open(dir, "/ext/subghz")) {
        FileInfo file_info;
        char filename[APP_MAX_PATH_LEN];
        char full_path[FULL_PATH_LEN];

        while(storage_dir_read(dir, &file_info, filename, sizeof(filename)) &&
              app_state.db.count < MAX_FILES) {
            popup_set_text(app_state.popup, filename, 64, 32, AlignCenter, AlignCenter);
            if(!file_info_is_dir(&file_info)) {
                snprintf(full_path, sizeof(full_path), "/ext/subghz/%s", filename);
                strncpy(
                    app_state.db.records[app_state.db.count].path,
                    filename,
                    sizeof(app_state.db.records[app_state.db.count].path) - 1);
                app_state.db.records[app_state.db.count].hash =
                    compute_file_hash(storage, full_path);
                app_state.db.count++;
            }
        }
        storage_dir_close(dir);

        process_duplicates(&app_state.db);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return 0;
}

void main_submenu_callback(void* context, uint32_t index) {
    UNUSED(context);
    switch(index) {
    case SubDupFinderSubmenuIndexScan:
        popup_set_text(app_state.popup, "Scanning...", 64, 32, AlignCenter, AlignCenter);
        view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewPopup);

        scan_thread_callback(NULL);

        render_groups_submenu();

        if(app_state.db.num_groups == 0) {
            popup_set_text(app_state.popup, "No duplicates", 64, 32, AlignCenter, AlignCenter);
        } else {
            view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewGroups);
        }
        break;
    case SubDupFinderSubmenuIndexCredits:
        view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewCredits);
        break;
    }
}

int32_t sub_dup_finder_app(void* p) {
    UNUSED(p);

    app_state.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app_state.view_dispatcher);
    app_state.main_submenu = submenu_alloc();
    app_state.groups_submenu = submenu_alloc();
    app_state.files_in_group_submenu = submenu_alloc();
    app_state.confirm_dialog = dialog_ex_alloc();
    app_state.credits_widget = widget_alloc();
    app_state.popup = popup_alloc();

    // Callbacks
    view_set_previous_callback(submenu_get_view(app_state.main_submenu), exit_callback);
    view_set_previous_callback(submenu_get_view(app_state.groups_submenu), back_to_submenu);
    view_set_previous_callback(submenu_get_view(app_state.files_in_group_submenu), back_to_groups);
    view_set_previous_callback(dialog_ex_get_view(app_state.confirm_dialog), back_to_files);
    view_set_previous_callback(widget_get_view(app_state.credits_widget), back_to_submenu);
    view_set_previous_callback(popup_get_view(app_state.popup), back_to_submenu);

    widget_add_string_element(
        app_state.credits_widget,
        0,
        10,
        AlignLeft,
        AlignTop,
        FontPrimary,
        "Sub Duplicate Finder v" APP_VERSION);
    widget_add_string_element(
        app_state.credits_widget, 0, 25, AlignLeft, AlignTop, FontPrimary, "Author: Endika");
    widget_add_string_element(
        app_state.credits_widget,
        0,
        38,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "https://github.com/endika/");
    widget_add_string_element(
        app_state.credits_widget, 0, 48, AlignLeft, AlignTop, FontSecondary, "flipper-sub-dup");

    submenu_add_item(
        app_state.main_submenu,
        "Find Duplicates",
        SubDupFinderSubmenuIndexScan,
        main_submenu_callback,
        NULL);
    submenu_add_item(
        app_state.main_submenu,
        "Credits",
        SubDupFinderSubmenuIndexCredits,
        main_submenu_callback,
        NULL);

    dialog_ex_set_right_button_text(app_state.confirm_dialog, "Yes");
    dialog_ex_set_left_button_text(app_state.confirm_dialog, "No");
    dialog_ex_set_result_callback(app_state.confirm_dialog, confirm_callback);

    view_dispatcher_add_view(
        app_state.view_dispatcher,
        SubDupFinderViewSubmenu,
        submenu_get_view(app_state.main_submenu));
    view_dispatcher_add_view(
        app_state.view_dispatcher,
        SubDupFinderViewGroups,
        submenu_get_view(app_state.groups_submenu));
    view_dispatcher_add_view(
        app_state.view_dispatcher,
        SubDupFinderViewFiles,
        submenu_get_view(app_state.files_in_group_submenu));
    view_dispatcher_add_view(
        app_state.view_dispatcher,
        SubDupFinderViewConfirm,
        dialog_ex_get_view(app_state.confirm_dialog));
    view_dispatcher_add_view(
        app_state.view_dispatcher,
        SubDupFinderViewCredits,
        widget_get_view(app_state.credits_widget));
    view_dispatcher_add_view(
        app_state.view_dispatcher, SubDupFinderViewPopup, popup_get_view(app_state.popup));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app_state.view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_switch_to_view(app_state.view_dispatcher, SubDupFinderViewSubmenu);
    view_dispatcher_run(app_state.view_dispatcher);

    view_dispatcher_remove_view(app_state.view_dispatcher, SubDupFinderViewSubmenu);
    view_dispatcher_remove_view(app_state.view_dispatcher, SubDupFinderViewGroups);
    view_dispatcher_remove_view(app_state.view_dispatcher, SubDupFinderViewFiles);
    view_dispatcher_remove_view(app_state.view_dispatcher, SubDupFinderViewConfirm);
    view_dispatcher_remove_view(app_state.view_dispatcher, SubDupFinderViewCredits);
    view_dispatcher_remove_view(app_state.view_dispatcher, SubDupFinderViewPopup);
    submenu_free(app_state.main_submenu);
    submenu_free(app_state.groups_submenu);
    submenu_free(app_state.files_in_group_submenu);
    dialog_ex_free(app_state.confirm_dialog);
    widget_free(app_state.credits_widget);
    popup_free(app_state.popup);
    view_dispatcher_free(app_state.view_dispatcher);
    furi_record_close(RECORD_GUI);

    return 0;
}
