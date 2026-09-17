#include "../dfc_i.h"
#include "../dfc_credential_storage.h"
#include "dfc_credential.h"
#include <toolbox/path.h>

static const char* dfc_scene_file_select_load_error_message(DfcCredentialLoadStatus status) {
    switch(status) {
    case DfcCredentialLoadStatusUnsupportedFormat:
        return "File format\nunsupported";
    case DfcCredentialLoadStatusUnsupportedAuthMode:
        return "Auth mode\nunsupported";
    case DfcCredentialLoadStatusOpenFailed:
        return "Can not open\nfile";
    case DfcCredentialLoadStatusCapacity:
        return "Too large for\nthis build";
    case DfcCredentialLoadStatusMalformedFile:
    case DfcCredentialLoadStatusOk:
    default:
        return "Can not parse\nfile";
    }
}

static void dfc_scene_file_select_callback(void* context) {
    Dfc* dfc = context;
    DfcCredential* dfc_credential = dfc->credential;

    // Either encoding may be selected: text is compiled on load, binary is fed
    // to the emulator as it stands.
    if(!furi_string_end_with(dfc_credential_storage_load_path(), DFC_APP_EXTENSION) &&
       !furi_string_end_with(dfc_credential_storage_load_path(), DFC_BINARY_EXTENSION)) {
        dfc_credential_clear(dfc_credential);
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneMainMenu);
        return;
    }

    FuriString* filename = furi_string_alloc();
    path_extract_filename(dfc_credential_storage_load_path(), filename, true);
    snprintf(
        dfc_credential->name, sizeof(dfc_credential->name), "%s", furi_string_get_cstr(filename));
    furi_string_free(filename);

    DfcCredentialLoadStatus status = DfcCredentialLoadStatusOk;
    if(dfc_credential_load_selected_file(dfc_credential, &status)) {
        scene_manager_next_scene(dfc->scene_manager, DfcSceneSavedMenu);
    } else {
        dfc_credential_storage_show_error(dfc_scene_file_select_load_error_message(status));
        dfc_credential_clear(dfc_credential);
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneMainMenu);
    }
}

bool dfc_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void dfc_scene_file_select_on_enter(void* context) {
    Dfc* dfc = context;
    FuriString* path = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);

    file_browser_configure(
        dfc->file_browser,
        DFC_APP_EXTENSION,
        STORAGE_APP_DATA_PATH_PREFIX,
        true,
        true,
        &I_Nfc_10px,
        true);
    file_browser_set_callback(dfc->file_browser, dfc_scene_file_select_callback, dfc);
    file_browser_start(dfc->file_browser, path);
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewFileBrowser);

    furi_string_free(path);
}

void dfc_scene_file_select_on_exit(void* context) {
    Dfc* dfc = context;
    file_browser_stop(dfc->file_browser);
}
