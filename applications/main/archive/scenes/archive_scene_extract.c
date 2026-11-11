#include "../archive_i.h"
#include "../helpers/archive_browser.h"
#include <toolbox/tar/tar_archive.h>
#include <toolbox/path.h>
#include <storage/storage.h>

// Scene state: 0 = confirm prompt, 1 = result shown.
#define EXTRACT_STATE_CONFIRM (0UL)
#define EXTRACT_STATE_RESULT  (1UL)

static void
    archive_scene_extract_button_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

// Derive the output directory by stripping the archive extension from the path.
static bool archive_extract_dest_from_path(const char* archive_path, FuriString* dest) {
    furi_string_set_str(dest, archive_path);
    if(furi_string_end_withi_str(dest, ".tar.gz")) {
        furi_string_left(dest, furi_string_size(dest) - strlen(".tar.gz"));
    } else if(furi_string_end_withi_str(dest, ".tgz")) {
        furi_string_left(dest, furi_string_size(dest) - strlen(".tgz"));
    } else if(furi_string_end_withi_str(dest, ".tar")) {
        furi_string_left(dest, furi_string_size(dest) - strlen(".tar"));
    } else {
        return false;
    }

    const char* name = strrchr(furi_string_get_cstr(dest), '/');
    name = name ? name + 1 : furi_string_get_cstr(dest);
    return *name && strcmp(name, ".") && strcmp(name, "..");
}

// Check every entry before creating the destination. The callback skips entries
// on this first pass, so malformed archives cannot write any files during it.
static bool archive_extract_validate_entry(const char* name, bool is_directory, void* context) {
    UNUSED(is_directory);
    bool* valid = context;
    if(!name[0] || name[0] == '/') {
        *valid = false;
        return false;
    }

    for(const char* part = name; *part;) {
        const char* end = strchr(part, '/');
        if(!end) end = part + strlen(part);
        size_t len = end - part;
        // Reject traversal and FAT-ambiguous components (trailing dots/spaces).
        // A literal "." is safe and common in tar-created relative paths.
        if(!len || (len == 2 && part[0] == '.' && part[1] == '.') ||
           (unsigned char)part[0] <= ' ' || (unsigned char)end[-1] <= ' ' ||
           (end[-1] == '.' && !(len == 1 && part[0] == '.'))) {
            *valid = false;
        }
        for(const char* c = part; c < end; ++c) {
            if((unsigned char)*c < ' ' || strchr("\\<>*|\":?", *c)) *valid = false;
        }
        part = *end ? end + 1 : end;
    }
    return false;
}

static bool archive_extract_accept_entry(const char* name, bool is_directory, void* context) {
    // Recheck on the write pass in case another client changed the open archive.
    archive_extract_validate_entry(name, is_directory, context);
    return *(bool*)context;
}

// Blocking unpack (mirrors how the OTA updater / storage_cli extract). Runs while
// the loading popup is shown on the stack view, which is drawn by the GUI service
// thread independently of this (blocking) app thread.
static bool archive_extract_perform(const char* archive_path, const char* dest_dir) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    TarArchive* tar = tar_archive_alloc(storage);
    bool ok = false;
    // Match the menu's case-insensitive extensions, including .TGZ/.TAR.GZ.
    FuriString* path = furi_string_alloc_set_str(archive_path);
    TarOpenMode mode =
        (furi_string_end_withi_str(path, ".tgz") || furi_string_end_withi_str(path, ".tar.gz")) ?
            TarOpenModeReadGzip :
            TarOpenModeRead;
    furi_string_free(path);
    if(tar_archive_open(tar, archive_path, mode)) {
        bool valid = true;
        tar_archive_set_file_callback(tar, archive_extract_validate_entry, &valid);
        ok = tar_archive_unpack_to(tar, dest_dir, NULL) && valid;
        // Never merge into an existing folder or replace the user's files.
        if(ok) ok = storage_common_mkdir(storage, dest_dir) == FSE_OK;
        if(ok) {
            tar_archive_set_file_callback(tar, archive_extract_accept_entry, &valid);
            ok = tar_archive_unpack_to(tar, dest_dir, NULL) && valid;
        }
    }
    tar_archive_free(tar);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void archive_scene_extract_on_enter(void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;

    ArchiveFile_t current;
    ArchiveFile_t_init(&current);
    app->extract_path = NULL;
    if(archive_copy_current_file(app->browser, &current) &&
       current.type != ArchiveFileTypeFolder) {
        app->extract_path = furi_string_alloc_set(current.path);
    }
    ArchiveFile_t_clear(&current);
    widget_reset(app->widget);
    if(!app->extract_path) {
        widget_add_text_box_element(
            app->widget, 0, 0, 128, 40, AlignCenter, AlignCenter, "File unavailable", false);
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "OK", archive_scene_extract_button_callback, app);
        scene_manager_set_scene_state(
            app->scene_manager, ArchiveAppSceneExtract, EXTRACT_STATE_RESULT);
        view_dispatcher_switch_to_view(app->view_dispatcher, ArchiveViewWidget);
        return;
    }
    FuriString* filename = furi_string_alloc();
    path_extract_filename(app->extract_path, filename, false);

    char prompt[96];
    snprintf(prompt, sizeof(prompt), "\e#Extract %s?\e#", furi_string_get_cstr(filename));

    widget_add_text_box_element(
        app->widget, 0, 0, 128, 40, AlignCenter, AlignCenter, prompt, false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", archive_scene_extract_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Extract", archive_scene_extract_button_callback, app);

    furi_string_free(filename);
    view_dispatcher_switch_to_view(app->view_dispatcher, ArchiveViewWidget);
}

bool archive_scene_extract_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t state = scene_manager_get_scene_state(app->scene_manager, ArchiveAppSceneExtract);

        if(state == EXTRACT_STATE_CONFIRM && event.event == GuiButtonTypeRight) {
            FuriString* dest = furi_string_alloc();
            bool ok = app->extract_path && archive_extract_dest_from_path(
                                               furi_string_get_cstr(app->extract_path), dest);

            view_dispatcher_switch_to_view(app->view_dispatcher, ArchiveViewStack);
            archive_show_loading_popup(app, true);
            if(ok) {
                ok = archive_extract_perform(
                    furi_string_get_cstr(app->extract_path), furi_string_get_cstr(dest));
            }
            archive_show_loading_popup(app, false);

            widget_reset(app->widget);
            char result_str[96];
            if(ok) {
                snprintf(
                    result_str,
                    sizeof(result_str),
                    "\e#Extracted to\e#\n%s",
                    furi_string_get_cstr(dest));
            } else {
                snprintf(
                    result_str,
                    sizeof(result_str),
                    "\e#Extract failed\e#\nInvalid archive, existing\nfolder, or storage error");
            }
            widget_add_text_box_element(
                app->widget, 0, 0, 128, 40, AlignCenter, AlignCenter, result_str, false);
            widget_add_button_element(
                app->widget, GuiButtonTypeCenter, "OK", archive_scene_extract_button_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, ArchiveViewWidget);

            scene_manager_set_scene_state(
                app->scene_manager, ArchiveAppSceneExtract, EXTRACT_STATE_RESULT);

            furi_string_free(dest);
            consumed = true;
        } else if(state == EXTRACT_STATE_CONFIRM && event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(app->scene_manager);
        } else if(state == EXTRACT_STATE_RESULT) {
            // Any button (OK) returns to the browser.
            consumed = scene_manager_previous_scene(app->scene_manager);
        }
    }
    return consumed;
}

void archive_scene_extract_on_exit(void* context) {
    furi_assert(context);
    ArchiveApp* app = (ArchiveApp*)context;
    scene_manager_set_scene_state(
        app->scene_manager, ArchiveAppSceneExtract, EXTRACT_STATE_CONFIRM);
    widget_reset(app->widget);
    if(app->extract_path) {
        furi_string_free(app->extract_path);
        app->extract_path = NULL;
    }
}
