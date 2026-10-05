#include "dnd_backup_storage.h"
#include "dnd_data.h"
#include "dnd_fs.h"
#include "dnd_profile_handoff.h"
#include "dnd_storage.h"

#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include "dndbackup_icons.h"
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DNDBACKUP_VIEW_MAIN       0U
#define DNDBACKUP_VIEW_TEXT       1U
#define DNDBACKUP_SETTINGS        "/ext/apps_data/dndolphins/backup_restore.txt"
#define DNDBACKUP_SETTINGS_TEMP   "/ext/apps_data/dndolphins/backup_restore.tmp"
#define DNDBACKUP_SETTINGS_BACKUP "/ext/apps_data/dndolphins/backup_restore.bak"
#define DNDBACKUP_DEFAULT_DIR     "/ext/apps_data/dndolphins/backups"

typedef enum {
    DndBackupEditNone,
    DndBackupEditFolder,
} DndBackupEdit;

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;
    TextInput* text_input;
    uint32_t profile;
    uint8_t have_profile;
    uint8_t selection;
    uint8_t scroll;
    uint8_t return_to_parent;
    DndBackupEdit edit;
    char backup_dir[DND_FS_LONG_PATH_LEN];
    char edit_buffer[DND_FS_LONG_PATH_LEN];
    char status[32];
} DndBackupApp;

static void dndbackup_copy(char* out, size_t size, const char* in) {
    if(!out || !size) return;
    const char* source = in ? in : "";
    size_t length = strlen(source);
    if(length >= size) length = size - 1U;
    memcpy(out, source, length);
    out[length] = '\0';
}

static bool dndbackup_valid_external_path(const char* path) {
    return dnd_backup_storage_valid_directory(path);
}

static void dndbackup_set_status(DndBackupApp* app, const char* status) {
    if(app) dndbackup_copy(app->status, sizeof(app->status), status);
}

static void dndbackup_load_settings(DndBackupApp* app) {
    if(!app) return;
    dndbackup_copy(app->backup_dir, sizeof(app->backup_dir), DNDBACKUP_DEFAULT_DIR);
    if(!app->storage || !storage_file_exists(app->storage, DNDBACKUP_SETTINGS)) return;
    File* file = storage_file_alloc(app->storage);
    if(!file) return;
    if(storage_file_open(file, DNDBACKUP_SETTINGS, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[DND_FS_LONG_PATH_LEN];
        size_t used = 0U;
        char ch = '\0';
        while(storage_file_read(file, &ch, 1U) == 1U && ch != '\n' && used + 1U < sizeof(line)) {
            if(ch != '\r') line[used++] = ch;
        }
        line[used] = '\0';
        if(!strncmp(line, "BackupDir=", 10U) && dndbackup_valid_external_path(line + 10U))
            dndbackup_copy(app->backup_dir, sizeof(app->backup_dir), line + 10U);
        storage_file_close(file);
    }
    storage_file_free(file);
}

static bool dndbackup_save_settings(DndBackupApp* app) {
    if(!app || !app->storage || !dndbackup_valid_external_path(app->backup_dir)) return false;
    Storage* storage = app->storage;
    storage_common_remove(storage, DNDBACKUP_SETTINGS_TEMP);
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, DNDBACKUP_SETTINGS_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        char line[DND_FS_LONG_PATH_LEN + 16U];
        int n = snprintf(line, sizeof(line), "BackupDir=%s\n", app->backup_dir);
        ok = n > 0 && (size_t)n < sizeof(line) &&
             storage_file_write(file, line, (size_t)n) == (size_t)n && storage_file_sync(file);
        storage_file_close(file);
    }
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(storage, DNDBACKUP_SETTINGS_TEMP);
        return false;
    }
    if(storage_file_exists(storage, DNDBACKUP_SETTINGS_BACKUP) &&
       storage_common_remove(storage, DNDBACKUP_SETTINGS_BACKUP) != FSE_OK) {
        storage_common_remove(storage, DNDBACKUP_SETTINGS_TEMP);
        return false;
    }
    bool had_live = storage_file_exists(storage, DNDBACKUP_SETTINGS);
    if(had_live &&
       storage_common_rename(storage, DNDBACKUP_SETTINGS, DNDBACKUP_SETTINGS_BACKUP) != FSE_OK) {
        storage_common_remove(storage, DNDBACKUP_SETTINGS_TEMP);
        return false;
    }
    if(storage_common_rename(storage, DNDBACKUP_SETTINGS_TEMP, DNDBACKUP_SETTINGS) == FSE_OK) {
        if(had_live) storage_common_remove(storage, DNDBACKUP_SETTINGS_BACKUP);
        return true;
    }
    if(had_live)
        (void)storage_common_rename(storage, DNDBACKUP_SETTINGS_BACKUP, DNDBACKUP_SETTINGS);
    storage_common_remove(storage, DNDBACKUP_SETTINGS_TEMP);
    return false;
}

static void dndbackup_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = 11U + row * 10U;
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_font(canvas, FontSecondary);
    char display[28];
    dndbackup_copy(display, sizeof(display), text);
    canvas_draw_str(canvas, 3, y + 8U, display);
    canvas_set_color(canvas, ColorBlack);
}

static void dndbackup_draw_header(Canvas* canvas, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    canvas_set_color(canvas, ColorBlack);
}

static void dndbackup_draw(Canvas* canvas, void* model) {
    DndBackupApp* app = *(DndBackupApp**)model;
    canvas_clear(canvas);
    dndbackup_draw_header(canvas, "DNDBackup & Restore");
    if(!app->have_profile) {
        dndbackup_draw_row(canvas, 0U, false, "No active character");
        return;
    }
    char rows[7][64];
    snprintf(rows[0], sizeof(rows[0]), "Profile [%lu]", (unsigned long)app->profile);
    snprintf(rows[1], sizeof(rows[1]), "Folder: %.43s", app->backup_dir);
    dndbackup_copy(rows[2], sizeof(rows[2]), "Create SHD Backup");
    dndbackup_copy(rows[3], sizeof(rows[3]), "Browse SHD Restore");
    dndbackup_copy(rows[4], sizeof(rows[4]), "Clone Active Character");
    dndbackup_copy(rows[5], sizeof(rows[5]), "Validate Character");
    dndbackup_copy(rows[6], sizeof(rows[6]), "Return to DNDolphins");
    for(uint8_t row = 0U; row < 5U; ++row) {
        uint8_t index = (uint8_t)(app->scroll + row);
        if(index >= 7U) break;
        dndbackup_draw_row(canvas, row, index == app->selection, rows[index]);
    }
    if(app->status[0]) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 63, app->status);
    }
}

static void dndbackup_redraw(DndBackupApp* app) {
    if(!app || !app->view) return;
    DndBackupApp** model = view_get_model(app->view);
    if(!model) return;
    *model = app;
    view_commit_model(app->view, true);
}

static void dndbackup_text_done(void* context) {
    DndBackupApp* app = context;
    if(!app) return;
    if(app->edit == DndBackupEditFolder) {
        if(!dndbackup_valid_external_path(app->edit_buffer)) {
            dndbackup_set_status(app, "Folder must be /ext");
        } else {
            dndbackup_copy(app->backup_dir, sizeof(app->backup_dir), app->edit_buffer);
            dndbackup_set_status(
                app, dndbackup_save_settings(app) ? "Backup folder saved" : "Folder save failed");
        }
    }
    app->edit = DndBackupEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, DNDBACKUP_VIEW_MAIN);
    dndbackup_redraw(app);
}

static void dndbackup_begin_folder_edit(DndBackupApp* app) {
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            dndbackup_set_status(app, "Text memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, DNDBACKUP_VIEW_TEXT, text_input_get_view(app->text_input));
    }
    app->edit = DndBackupEditFolder;
    dndbackup_copy(app->edit_buffer, sizeof(app->edit_buffer), app->backup_dir);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Backup folder");
    text_input_set_result_callback(
        app->text_input,
        dndbackup_text_done,
        app,
        app->edit_buffer,
        sizeof(app->edit_buffer),
        false);
    view_dispatcher_switch_to_view(app->dispatcher, DNDBACKUP_VIEW_TEXT);
}

static bool dndbackup_browse_and_restore(DndBackupApp* app) {
    if(!app || !app->dialogs) return false;
    FuriString* selected = furi_string_alloc_set(app->backup_dir);
    if(!selected) {
        dndbackup_set_status(app, "Browser memory low");
        return false;
    }
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".shd", &I_shd_sword_10x10);
    options.base_path = app->backup_dir;
    bool chosen = dialog_file_browser_show(app->dialogs, selected, selected, &options);
    bool ok = false;
    if(chosen) {
        DndSaveData* restored = calloc(1U, sizeof(DndSaveData));
        ok = restored && dnd_backup_storage_restore_bundle(
                             app->storage, app->profile, furi_string_get_cstr(selected), restored);
        if(ok) {
            DndProfileState profiles;
            memset(&profiles, 0, sizeof(profiles));
            ok = dnd_storage_profiles_load(app->storage, &profiles) &&
                 dnd_storage_profiles_refresh(app->storage, &profiles) &&
                 dnd_storage_profiles_save(app->storage, &profiles);
        }
        if(restored) {
            dnd_data_clear(restored);
            free(restored);
        }
        dndbackup_set_status(app, ok ? "SHD restore complete" : "Select core SHD file");
    }
    furi_string_free(selected);
    return chosen && ok;
}

static bool dndbackup_clone_active(DndBackupApp* app, uint32_t* new_profile) {
    if(new_profile) *new_profile = UINT32_MAX;
    if(!app || !app->storage || !app->have_profile) return false;
    DndProfileState profiles;
    memset(&profiles, 0, sizeof(profiles));
    if(!dnd_storage_profiles_load(app->storage, &profiles)) return false;
    uint32_t destination = dnd_storage_profiles_next_id(&profiles);
    bool ok = destination != UINT32_MAX && destination != app->profile &&
              dnd_storage_duplicate_profile(app->storage, app->profile, destination);
    if(ok) {
        bool indexed = dnd_storage_profiles_refresh(app->storage, &profiles) &&
                       dnd_storage_profiles_save(app->storage, &profiles);
        if(!indexed) {
            (void)dnd_storage_delete_profile(app->storage, destination);
            ok = false;
        }
    }
    if(ok && new_profile) *new_profile = destination;
    dnd_storage_profiles_free(&profiles);
    return ok;
}

static bool dndbackup_validate_active(DndBackupApp* app, char* status, size_t status_size) {
    if(!app || !app->storage || !app->have_profile || !status || !status_size) return false;
    bool ok = dnd_storage_verify_profile(app->storage, app->profile) &&
              dnd_storage_validate_profile_semantics(app->storage, app->profile);
    if(!ok) {
        dndbackup_copy(status, status_size, "Invalid character save");
        return false;
    }
    dndbackup_copy(status, status_size, "Character validates OK");
    return true;
}

static bool dndbackup_input(InputEvent* event, void* context) {
    DndBackupApp* app = context;
    if(!app || !event) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong)
        return true;

    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->return_to_parent = 0U;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->return_to_parent = 1U;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(!app->have_profile) return true;

    bool move = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(move && event->key == InputKeyUp) {
        app->selection = app->selection > 1U ? app->selection - 1U : 6U;
    } else if(move && event->key == InputKeyDown) {
        app->selection = app->selection < 6U ? app->selection + 1U : 1U;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 1U) {
            dndbackup_begin_folder_edit(app);
            return true;
        } else if(app->selection == 2U) {
            dndbackup_set_status(
                app,
                dnd_backup_storage_export_bundle(app->storage, app->profile, app->backup_dir) ?
                    "SHD backup written" :
                    "SHD backup failed");
        } else if(app->selection == 3U) {
            (void)dndbackup_browse_and_restore(app);
        } else if(app->selection == 4U) {
            uint32_t clone = UINT32_MAX;
            if(dndbackup_clone_active(app, &clone))
                snprintf(
                    app->status, sizeof(app->status), "Cloned as [%lu]", (unsigned long)clone);
            else
                dndbackup_set_status(app, "Clone failed");
        } else if(app->selection == 5U) {
            char validation[32];
            (void)dndbackup_validate_active(app, validation, sizeof(validation));
            dndbackup_set_status(app, validation);
        } else if(app->selection == 6U) {
            app->return_to_parent = 1U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        }
    }
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = (uint8_t)(app->selection - 4U);
    if(app->scroll > 2U) app->scroll = 2U;
    dndbackup_redraw(app);
    return true;
}

int32_t dndbackup_app(void* context) {
    (void)context;
    DndBackupApp* app = calloc(1U, sizeof(DndBackupApp));
    if(!app) return -1;
    app->return_to_parent = 1U;
    app->selection = 1U;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    if(!app->gui || !app->storage || !app->dialogs) goto cleanup;
    app->have_profile = dnd_profile_ref_active_id(app->storage, &app->profile) &&
                        dnd_profile_ref_exists(app->storage, app->profile);
    dndbackup_load_settings(app);
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto cleanup;
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndBackupApp*));
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndbackup_draw);
    view_set_input_callback(app->view, dndbackup_input);
    DndBackupApp** model = view_get_model(app->view);
    if(!model) goto cleanup;
    *model = app;
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->dispatcher, DNDBACKUP_VIEW_MAIN, app->view);
    view_dispatcher_switch_to_view(app->dispatcher, DNDBACKUP_VIEW_MAIN);
    dnd_handoff_ready(DNDBACKUP_FAP_PATH);
    view_dispatcher_run(app->dispatcher);

cleanup: {
    bool return_to_parent = app->return_to_parent != 0U;
    if(return_to_parent)
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_CHARACTER);

    if(app->text_input) {
        if(app->dispatcher) view_dispatcher_remove_view(app->dispatcher, DNDBACKUP_VIEW_TEXT);
        text_input_free(app->text_input);
    }
    if(app->view) {
        if(app->dispatcher) view_dispatcher_remove_view(app->dispatcher, DNDBACKUP_VIEW_MAIN);
        view_free(app->view);
    }
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->dialogs) furi_record_close(RECORD_DIALOGS);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}
    return 0;
}
