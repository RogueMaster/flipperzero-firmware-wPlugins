#include "../tpms_app_i.h"
#include "../tpms_save.h"

#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TPMS_SAVED_PAGE_SIZE 5U
#define TPMS_SAVED_PREVIOUS 0xFFFFFFFEUL
#define TPMS_SAVED_NEXT 0xFFFFFFFFUL
#define TPMS_SAVED_ORG 0xFFFFFFFCUL
#define TPMS_SAVED_MOD 0xFFFFFFFDUL

typedef enum {
    TPMSSavedSourceNone = 0,
    TPMSSavedSourceOrg,
    TPMSSavedSourceMod,
} TPMSSavedSource;

static FuriString* tpms_saved_paths[TPMS_SAVED_PAGE_SIZE];
static uint8_t tpms_saved_count = 0U;
static uint16_t tpms_saved_page = 0U;
static uint16_t tpms_saved_total = 0U;
static TPMSSavedSource tpms_saved_source = TPMSSavedSourceNone;

static bool tpms_saved_has_extension(const char* name) {
    const size_t len = name ? strlen(name) : 0U;
    if(len < 5U) return false;
    const char* ext = name + len - 5U;
    return ext[0] == '.' &&
           (ext[1] == 't' || ext[1] == 'T') &&
           (ext[2] == 'p' || ext[2] == 'P') &&
           (ext[3] == 'm' || ext[3] == 'M') &&
           (ext[4] == 's' || ext[4] == 'S');
}

static void tpms_saved_clear(void) {
    for(uint8_t i = 0U; i < TPMS_SAVED_PAGE_SIZE; i++) {
        if(tpms_saved_paths[i]) {
            furi_string_free(tpms_saved_paths[i]);
            tpms_saved_paths[i] = NULL;
        }
    }
    tpms_saved_count = 0U;
}

static void tpms_saved_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(app) view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static uint16_t tpms_saved_scan(TPMSApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return 0U;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_PROFILE_FOLDER);
    storage_common_mkdir(storage, TPMS_PROFILE_RX_FOLDER);
    storage_common_mkdir(storage, TPMS_PROFILE_EDITED_FOLDER);
    const char* folder = tpms_saved_source == TPMSSavedSourceMod ?
                             TPMS_PROFILE_EDITED_FOLDER : TPMS_PROFILE_RX_FOLDER;

    File* dir = storage_file_alloc(storage);
    uint16_t total = 0U;
    if(dir && storage_dir_open(dir, folder)) {
        FileInfo info;
        char name[96];
        const uint32_t page_start = (uint32_t)tpms_saved_page * TPMS_SAVED_PAGE_SIZE;
        const uint32_t page_end = page_start + TPMS_SAVED_PAGE_SIZE;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info) || !tpms_saved_has_extension(name)) continue;
            const uint32_t global_index = total++;
            if(global_index >= page_start && global_index < page_end &&
               tpms_saved_count < TPMS_SAVED_PAGE_SIZE) {
                FuriString* path = furi_string_alloc();
                if(!path) continue;
                furi_string_printf(path, "%s/%s", folder, name);
                tpms_saved_paths[tpms_saved_count] = path;

                char label[32];
                uint32_t saved_id = 0U;
                char display[64];
                if(tpms_profile_read_label(
                       furi_string_get_cstr(path), label, sizeof(label), &saved_id)) {
                    if(saved_id != 0U)
                        snprintf(display, sizeof(display), "%s %08lX", label, (unsigned long)saved_id);
                    else
                        snprintf(display, sizeof(display), "%s", label);
                    submenu_add_item(
                        app->submenu, display, tpms_saved_count, tpms_saved_callback, app);
                } else {
                    submenu_add_item(
                        app->submenu, name, tpms_saved_count, tpms_saved_callback, app);
                }
                tpms_saved_count++;
            }
        }
        storage_dir_close(dir);
    }
    if(dir) storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return total;
}

static void tpms_saved_build_list(TPMSApp* app) {
    tpms_saved_clear();
    submenu_reset(app->submenu);
    uint16_t total = tpms_saved_scan(app);
    if(total > 0U && (uint32_t)tpms_saved_page * TPMS_SAVED_PAGE_SIZE >= total &&
       tpms_saved_page > 0U) {
        tpms_saved_page = (uint16_t)((total - 1U) / TPMS_SAVED_PAGE_SIZE);
        tpms_saved_clear();
        submenu_reset(app->submenu);
        total = tpms_saved_scan(app);
    }
    tpms_saved_total = total;
    if(total == 0U) {
        tpms_saved_page = 0U;
        submenu_set_header(app->submenu, "No saved TPMS");
    } else {
        const uint16_t pages = (uint16_t)((total + TPMS_SAVED_PAGE_SIZE - 1U) / TPMS_SAVED_PAGE_SIZE);
        char header[32];
        snprintf(
            header,
            sizeof(header),
            "%s %u/%u",
            tpms_saved_source == TPMSSavedSourceMod ? "MOD" : "ORG",
            (unsigned)(tpms_saved_page + 1U),
            (unsigned)pages);
        submenu_set_header(app->submenu, header);
        if(tpms_saved_page > 0U) {
            submenu_add_item(app->submenu, "< Previous", TPMS_SAVED_PREVIOUS, tpms_saved_callback, app);
        }
        if(tpms_saved_page + 1U < pages) {
            submenu_add_item(app->submenu, "Next >", TPMS_SAVED_NEXT, tpms_saved_callback, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewSubmenu);
}

static void tpms_saved_build_source_menu(TPMSApp* app) {
    tpms_saved_clear();
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Load saved");
    submenu_add_item(app->submenu, "ORG", TPMS_SAVED_ORG, tpms_saved_callback, app);
    submenu_add_item(app->submenu, "MOD", TPMS_SAVED_MOD, tpms_saved_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewSubmenu);
}

void tpms_scene_saved_on_enter(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    app->editor_profile_loaded = false;
    tpms_saved_source = TPMSSavedSourceNone;
    tpms_saved_page = 0U;
    tpms_saved_build_source_menu(app);
}

bool tpms_scene_saved_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeBack && tpms_saved_source != TPMSSavedSourceNone) {
        tpms_saved_source = TPMSSavedSourceNone;
        tpms_saved_page = 0U;
        tpms_saved_build_source_menu(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == TPMS_SAVED_ORG || event.event == TPMS_SAVED_MOD) {
        tpms_saved_source = event.event == TPMS_SAVED_MOD ?
                                TPMSSavedSourceMod : TPMSSavedSourceOrg;
        tpms_saved_page = 0U;
        tpms_saved_build_list(app);
        return true;
    }
    if(event.event == TPMS_SAVED_PREVIOUS) {
        if(tpms_saved_page > 0U) tpms_saved_page--;
        tpms_saved_build_list(app);
        return true;
    }
    if(event.event == TPMS_SAVED_NEXT) {
        const uint32_t next = ((uint32_t)tpms_saved_page + 1U) * TPMS_SAVED_PAGE_SIZE;
        if(next < tpms_saved_total) tpms_saved_page++;
        tpms_saved_build_list(app);
        return true;
    }
    if(event.event >= tpms_saved_count || !tpms_saved_paths[event.event]) return false;

    memset(&app->editor_profile_values, 0, sizeof(app->editor_profile_values));
    memset(app->editor_profile_protocol, 0, sizeof(app->editor_profile_protocol));
    if(tpms_profile_load(
           furi_string_get_cstr(tpms_saved_paths[event.event]),
           &app->editor_profile_values,
           app->editor_profile_protocol,
           sizeof(app->editor_profile_protocol))) {
        app->editor_profile_loaded = true;
        scene_manager_next_scene(app->scene_manager, TPMSSceneEditor);
    } else {
        notification_message(app->notifications, &sequence_blink_red_10);
    }
    return true;
}

void tpms_scene_saved_on_exit(void* context) {
    TPMSApp* app = context;
    if(app && app->submenu) submenu_reset(app->submenu);
    tpms_saved_clear();
    tpms_saved_source = TPMSSavedSourceNone;
    tpms_saved_page = 0U;
}
