#include "../bad_usb_app_i.h"
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Les operations sont exclusivement limitees a /ext/apps_data/badusb_msc/files/.
static const char* files_name(const FuriString* path) {
    const char* p = furi_string_get_cstr(path);
    const char* slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}

bool bad_usb_files_path_valid(const FuriString* path) {
    const char* full = furi_string_get_cstr(path);
    const size_t len = strlen(BAD_USB_FILES_FOLDER);
    if(strncmp(full, BAD_USB_FILES_FOLDER, len) || full[len] != '/') return false;
    const char* relative = full + len + 1;
    if(!*relative || strstr(relative, "../") || strstr(relative, "/..") ||
       strstr(relative, "//") || !strcmp(relative, ".."))
        return false;
    return true;
}

void bad_usb_files_menu_callback(void* context, uint32_t index) {
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_files_input_callback(void* context) {
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BadUsbFilesEventInputDone);
}

void bad_usb_files_message(BadUsbApp* app, const char* header, const char* message) {
    DialogMessage* dialog = dialog_message_alloc();
    dialog_message_set_header(dialog, header, 64, 8, AlignCenter, AlignTop);
    dialog_message_set_text(dialog, message, 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(dialog, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, dialog);
    dialog_message_free(dialog);
}

// -------------------------------- Menu ------------------------------------
void bad_usb_scene_home_on_enter(void* context) {
    BadUsbApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "BadUSB MSC");
    submenu_add_item(
        app->submenu, "BadUSB", BadUsbFilesEventScripts, bad_usb_files_menu_callback, app);
    submenu_add_item(
        app->submenu, "Files", BadUsbFilesEventFiles, bad_usb_files_menu_callback, app);
    // Keyboard Layout remains available from BadUSB -> Config.
    if(app->storage_show_progress) {
        bad_usb_storage_progress_update(app, 0, 0);
        view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewStorageProgress);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewSubmenu);
    }
}

bool bad_usb_scene_home_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(app->storage_show_progress) {
        if(event.type == SceneManagerEventTypeTick) {
            if(app->storage_prepare_thread &&
               furi_thread_get_state(app->storage_prepare_thread) == FuriThreadStateStopped) {
                furi_thread_join(app->storage_prepare_thread);
                app->storage_prepare_result =
                    furi_thread_get_return_code(app->storage_prepare_thread);
                app->storage_prepared = app->storage_prepare_result == ComboFat16PrepareReady;
                furi_thread_free(app->storage_prepare_thread);
                app->storage_prepare_thread = NULL;
                app->storage_done_ticks = 0;
            }
            if(!app->storage_prepare_thread) {
                bool ok = app->storage_prepared;
                bad_usb_storage_progress_update(
                    app,
                    ok ? 100 : __atomic_load_n(&app->storage_percent, __ATOMIC_RELAXED),
                    ok ? 1 : 2);
                if(++app->storage_done_ticks >= 4) {
                    app->storage_show_progress = false;
                    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewSubmenu);
                    if(!ok)
                        bad_usb_files_message(app, "Storage Error", "Check microSD / drive.img");
                }
            } else {
                bad_usb_storage_progress_update(
                    app, __atomic_load_n(&app->storage_percent, __ATOMIC_RELAXED), 0);
            }
        }
        // Prevent opening BadUSB or leaving the progress screen while writing.
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == BadUsbFilesEventScripts) {
        furi_string_set(app->file_path, BAD_USB_APP_BASE_FOLDER);
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFileSelect);
        return true;
    } else if(event.event == BadUsbFilesEventFiles) {
        furi_string_set(app->storage_file_path, BAD_USB_FILES_FOLDER);
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesSelect);
        return true;
    }
    return false;
}
void bad_usb_scene_home_on_exit(void* context) {
    UNUSED(context);
}

// ---------------------------- Selection fichier --------------------------
void bad_usb_scene_files_select_on_enter(void* context) {
    BadUsbApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    // Le chemin est cree une seule fois et utilise dans qFlipper.
    storage_common_mkdir(storage, BAD_USB_FILES_BASE_FOLDER);
    storage_common_mkdir(storage, BAD_USB_FILES_FOLDER);
    bool available = storage_dir_exists(storage, BAD_USB_FILES_FOLDER);
    furi_record_close(RECORD_STORAGE);
    if(!available) {
        bad_usb_files_message(app, "No microSD", "Check the SD card");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!bad_usb_files_path_valid(app->storage_file_path)) {
        furi_string_set(app->storage_file_path, BAD_USB_FILES_FOLDER);
    }
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, "*", NULL);
    options.base_path = BAD_USB_FILES_FOLDER;
    options.hide_ext = false;
    options.skip_assets = false;
    options.hide_dot_files = true;

    if(dialog_file_browser_show(
           app->dialogs, app->storage_file_path, app->storage_file_path, &options) &&
       bad_usb_files_path_valid(app->storage_file_path)) {
        app->read_offset = 0;
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesActions);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}
bool bad_usb_scene_files_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void bad_usb_scene_files_select_on_exit(void* context) {
    UNUSED(context);
}

// ------------------------------ Actions ----------------------------------
static void files_refresh_size(BadUsbApp* app) {
    app->selected_size = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    if(storage_common_stat(storage, furi_string_get_cstr(app->storage_file_path), &info) ==
           FSE_OK &&
       !file_info_is_dir(&info))
        app->selected_size = info.size;
    furi_record_close(RECORD_STORAGE);
}

void bad_usb_scene_files_actions_on_enter(void* context) {
    BadUsbApp* app = context;
    files_refresh_size(app);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, files_name(app->storage_file_path));
    submenu_add_item(app->submenu, "Read", BadUsbFilesEventRead, bad_usb_files_menu_callback, app);
    if(app->read_offset + BAD_USB_FILES_PAGE_SIZE < app->selected_size) {
        submenu_add_item(
            app->submenu, "Next page", BadUsbFilesEventNextPage, bad_usb_files_menu_callback, app);
    }
    if(app->read_offset > 0) {
        submenu_add_item(
            app->submenu,
            "Previous page",
            BadUsbFilesEventPrevPage,
            bad_usb_files_menu_callback,
            app);
    }
    submenu_add_item(
        app->submenu, "Edit line", BadUsbFilesEventEdit, bad_usb_files_menu_callback, app);
    submenu_add_item(
        app->submenu, "Rename", BadUsbFilesEventRename, bad_usb_files_menu_callback, app);
    submenu_add_item(
        app->submenu, "Delete", BadUsbFilesEventDelete, bad_usb_files_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewSubmenu);
}

static void files_delete(BadUsbApp* app) {
    DialogMessage* dialog = dialog_message_alloc();
    dialog_message_set_header(dialog, "Delete this file?", 64, 7, AlignCenter, AlignTop);
    dialog_message_set_text(
        dialog, files_name(app->storage_file_path), 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(dialog, "No", NULL, "Yes");
    DialogMessageButton result = dialog_message_show(app->dialogs, dialog);
    dialog_message_free(dialog);
    if(result != DialogMessageButtonRight) return;
    if(!bad_usb_files_path_valid(app->storage_file_path)) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error error = storage_common_remove(storage, furi_string_get_cstr(app->storage_file_path));
    furi_record_close(RECORD_STORAGE);
    if(error == FSE_OK) {
        bad_usb_files_message(app, "Deleted", "File removed");
        furi_string_set(app->storage_file_path, BAD_USB_FILES_FOLDER);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BadUsbSceneFilesSelect);
    } else
        bad_usb_files_message(app, "Error", "Delete failed");
}

bool bad_usb_scene_files_actions_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case BadUsbFilesEventRead:
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesRead);
        return true;
    case BadUsbFilesEventNextPage:
        app->read_offset += BAD_USB_FILES_PAGE_SIZE;
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesRead);
        return true;
    case BadUsbFilesEventPrevPage:
        if(app->read_offset >= BAD_USB_FILES_PAGE_SIZE)
            app->read_offset -= BAD_USB_FILES_PAGE_SIZE;
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesRead);
        return true;
    case BadUsbFilesEventEdit:
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesLines);
        return true;
    case BadUsbFilesEventRename:
        scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesRename);
        return true;
    case BadUsbFilesEventDelete:
        files_delete(app);
        return true;
    default:
        return false;
    }
}
void bad_usb_scene_files_actions_on_exit(void* context) {
    UNUSED(context);
}

// -------------------------------- Lire -----------------------------------
void bad_usb_scene_files_read_on_enter(void* context) {
    BadUsbApp* app = context;
    if(!bad_usb_files_path_valid(app->storage_file_path)) {
        bad_usb_files_message(app, "Error", "Invalid file path");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char* buf = app->read_buffer;
    size_t used = 0;
    if(storage_file_open(
           file, furi_string_get_cstr(app->storage_file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(app->read_offset >= size) app->read_offset = 0;
        if(storage_file_seek(file, app->read_offset, true)) {
            used = storage_file_read(file, buf, BAD_USB_FILES_PAGE_SIZE);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    // Remplacer les octets de controle pour rendre les fichiers binaires consultables sans crash.
    for(size_t i = 0; i < used; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if(c == 0 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) buf[i] = '.';
    }
    buf[used] = 0;
    if(app->read_offset + used < app->selected_size) {
        const char* marker = "\n\n[More: Back > Next page]";
        strlcat(buf, marker, BAD_USB_FILES_PAGE_SIZE + 180);
    }
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, buf);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewTextBox);
}
bool bad_usb_scene_files_read_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void bad_usb_scene_files_read_on_exit(void* context) {
    UNUSED(context);
}

// -------------------------------- Edition ligne par ligne ----------------
static bool files_load_for_edit(BadUsbApp* app) {
    if(!bad_usb_files_path_valid(app->storage_file_path)) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(
           file, furi_string_get_cstr(app->storage_file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size <= BAD_USB_FILES_EDIT_SIZE) {
            app->edit_length = size;
            ok = storage_file_read(file, app->edit_buffer, app->edit_length) == app->edit_length;
            if(ok && memchr(app->edit_buffer, '\0', app->edit_length)) ok = false;
            if(ok) app->edit_buffer[app->edit_length] = '\0';
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void bad_usb_scene_files_lines_on_enter(void* context) {
    BadUsbApp* app = context;
    if(!files_load_for_edit(app)) {
        bad_usb_files_message(app, "Read only", "Text up to 8 KB");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Edit a line");
    app->edit_lines = 0;
    size_t start = 0;
    for(size_t i = 0; i <= app->edit_length && app->edit_lines < BAD_USB_FILES_MAX_LINES; ++i) {
        if(i == app->edit_length || app->edit_buffer[i] == '\n') {
            size_t length = i - start;
            app->edit_line_starts[app->edit_lines] = start;
            app->edit_line_lengths[app->edit_lines] = length;
            char label[42];
            snprintf(
                label,
                sizeof(label),
                "%u: %.26s",
                (unsigned)(app->edit_lines + 1),
                app->edit_buffer + start);
            // Ne pas afficher du contenu des lignes suivantes dans l'extrait.
            char* p = label;
            while(*p) {
                if(*p == '\r' || *p == '\n') {
                    *p = 0;
                    break;
                }
                ++p;
            }
            submenu_add_item(
                app->submenu,
                label,
                BadUsbFilesEventLineBase + app->edit_lines,
                bad_usb_files_menu_callback,
                app);
            app->edit_lines++;
            start = i + 1;
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewSubmenu);
}
bool bad_usb_scene_files_lines_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    // SceneManagerEvent.event est un uint32_t : comparer avec des valeurs
    // non signees pour eviter -Werror=sign-compare dans ufbt.
    const uint32_t line_base = (uint32_t)BadUsbFilesEventLineBase;
    if(event.type == SceneManagerEventTypeCustom && event.event >= line_base &&
       (event.event - line_base) < (uint32_t)app->edit_lines) {
        app->edit_line_index = (uint16_t)(event.event - line_base);
        if(app->edit_line_lengths[app->edit_line_index] >= sizeof(app->line_buffer)) {
            bad_usb_files_message(app, "Line too long", "Edit in qFlipper");
        } else {
            scene_manager_next_scene(app->scene_manager, BadUsbSceneFilesEdit);
        }
        return true;
    }
    return false;
}
void bad_usb_scene_files_lines_on_exit(void* context) {
    UNUSED(context);
}

// Remplacement soigneux : fichier temporaire + copie de secours locale.
static bool files_save_line(BadUsbApp* app) {
    size_t pos = app->edit_line_starts[app->edit_line_index];
    size_t old_len = app->edit_line_lengths[app->edit_line_index];
    size_t new_len = strlen(app->line_buffer);
    if(pos + old_len > app->edit_length) return false;
    size_t new_size = app->edit_length - old_len + new_len;
    if(new_size > BAD_USB_FILES_EDIT_SIZE) return false;
    if(!bad_usb_files_path_valid(app->storage_file_path)) return false;

    FuriString* temp =
        furi_string_alloc_printf("%s.__badusb_tmp", furi_string_get_cstr(app->storage_file_path));
    FuriString* backup =
        furi_string_alloc_printf("%s.__badusb_bak", furi_string_get_cstr(app->storage_file_path));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;
    // Ne jamais ecraser un fichier temporaire ou une sauvegarde existants.
    if(!storage_file_exists(storage, furi_string_get_cstr(temp)) &&
       !storage_file_exists(storage, furi_string_get_cstr(backup))) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, furi_string_get_cstr(temp), FSAM_WRITE, FSOM_CREATE_NEW)) {
            bool write_ok =
                storage_file_write(file, app->edit_buffer, pos) == pos &&
                storage_file_write(file, app->line_buffer, new_len) == new_len &&
                storage_file_write(
                    file, app->edit_buffer + pos + old_len, app->edit_length - (pos + old_len)) ==
                    app->edit_length - (pos + old_len);
            write_ok = storage_file_sync(file) && write_ok;
            storage_file_close(file);
            if(write_ok && storage_common_rename(
                               storage,
                               furi_string_get_cstr(app->storage_file_path),
                               furi_string_get_cstr(backup)) == FSE_OK) {
                if(storage_common_rename(
                       storage,
                       furi_string_get_cstr(temp),
                       furi_string_get_cstr(app->storage_file_path)) == FSE_OK) {
                    storage_common_remove(storage, furi_string_get_cstr(backup));
                    ok = true;
                } else {
                    storage_common_rename(
                        storage,
                        furi_string_get_cstr(backup),
                        furi_string_get_cstr(app->storage_file_path));
                }
            }
            if(!ok) storage_common_remove(storage, furi_string_get_cstr(temp));
        }
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
    furi_string_free(temp);
    furi_string_free(backup);
    return ok;
}

void bad_usb_scene_files_edit_on_enter(void* context) {
    BadUsbApp* app = context;
    size_t start = app->edit_line_starts[app->edit_line_index];
    size_t length = app->edit_line_lengths[app->edit_line_index];
    if(length >= sizeof(app->line_buffer)) length = sizeof(app->line_buffer) - 1;
    memcpy(app->line_buffer, app->edit_buffer + start, length);
    app->line_buffer[length] = '\0';
    // Supprimer CR final des lignes CRLF; celui-ci sera restaure lors de la sauvegarde.
    if(length && app->line_buffer[length - 1] == '\r') app->line_buffer[length - 1] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Edit line");
    text_input_set_result_callback(
        app->text_input,
        bad_usb_files_input_callback,
        app,
        app->line_buffer,
        sizeof(app->line_buffer),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewTextInput);
}
bool bad_usb_scene_files_edit_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == BadUsbFilesEventInputDone) {
        // Retablir CR pour les fichiers Windows avec fins de lignes CRLF.
        size_t pos = app->edit_line_starts[app->edit_line_index];
        size_t len = app->edit_line_lengths[app->edit_line_index];
        if(len && app->edit_buffer[pos + len - 1] == '\r')
            strlcat(app->line_buffer, "\r", sizeof(app->line_buffer));
        if(files_save_line(app)) {
            bad_usb_files_message(app, "Saved", "Line updated");
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BadUsbSceneFilesActions);
        } else {
            bad_usb_files_message(app, "Error", "Save failed");
        }
        return true;
    }
    return false;
}
void bad_usb_scene_files_edit_on_exit(void* context) {
    UNUSED(context);
}

// -------------------------------- Renommer -------------------------------
static bool files_rename(BadUsbApp* app) {
    const char* name = app->rename_buffer;
    if(!*name || !strcmp(name, ".") || !strcmp(name, "..") || strchr(name, '/') ||
       strchr(name, '\\') || strchr(name, ':'))
        return false;
    if(!bad_usb_files_path_valid(app->storage_file_path)) return false;
    FuriString* dest = furi_string_alloc();
    FuriString* parent = furi_string_alloc();
    path_extract_dirname(furi_string_get_cstr(app->storage_file_path), parent);
    furi_string_printf(dest, "%s/%s", furi_string_get_cstr(parent), name);
    bool ok = bad_usb_files_path_valid(dest);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(ok && furi_string_cmp(dest, app->storage_file_path) != 0) {
        if(storage_file_exists(storage, furi_string_get_cstr(dest)) ||
           storage_dir_exists(storage, furi_string_get_cstr(dest)))
            ok = false;
        else
            ok = storage_common_rename(
                     storage,
                     furi_string_get_cstr(app->storage_file_path),
                     furi_string_get_cstr(dest)) == FSE_OK;
    }
    furi_record_close(RECORD_STORAGE);
    if(ok) furi_string_set(app->storage_file_path, dest);
    furi_string_free(dest);
    furi_string_free(parent);
    return ok;
}

void bad_usb_scene_files_rename_on_enter(void* context) {
    BadUsbApp* app = context;
    strlcpy(app->rename_buffer, files_name(app->storage_file_path), sizeof(app->rename_buffer));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Name and extension");
    text_input_set_result_callback(
        app->text_input,
        bad_usb_files_input_callback,
        app,
        app->rename_buffer,
        sizeof(app->rename_buffer),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewTextInput);
}
bool bad_usb_scene_files_rename_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == BadUsbFilesEventInputDone) {
        if(files_rename(app)) {
            bad_usb_files_message(app, "Renamed", "Name updated");
            scene_manager_previous_scene(app->scene_manager);
        } else
            bad_usb_files_message(app, "Error", "Invalid or duplicate");
        return true;
    }
    return false;
}
void bad_usb_scene_files_rename_on_exit(void* context) {
    UNUSED(context);
}
