#include "../tpms_app_i.h"
#include "../tpms_save.h"

#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TPMS_LAB_LIST_PAGE_SIZE 5U
#define TPMS_LAB_PREVIOUS 0xFFFFFFFEUL
#define TPMS_LAB_NEXT 0xFFFFFFFFUL

static FuriString* tpms_lab_saved_paths[TPMS_LAB_LIST_PAGE_SIZE];
static uint8_t tpms_lab_session_slots[TPMS_LAB_LIST_PAGE_SIZE];
static uint8_t tpms_lab_list_count = 0U;
static uint16_t tpms_lab_list_page = 0U;
static uint16_t tpms_lab_list_total = 0U;

static void tpms_lab_sensors_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(app) view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static const char* tpms_lab_sensor_protocol_short(TpmsEncoderKind kind) {
    const char* name = tpms_encoder_display_name(kind);
    return name ? name : "TPMS";
}

static bool tpms_lab_has_tpl_extension(const char* name) {
    const size_t len = name ? strlen(name) : 0U;
    if(len < 4U) return false;
    const char* ext = name + len - 4U;
    return ext[0] == '.' &&
           (ext[1] == 't' || ext[1] == 'T') &&
           (ext[2] == 'p' || ext[2] == 'P') &&
           (ext[3] == 'l' || ext[3] == 'L');
}

static void tpms_lab_list_clear(void) {
    for(uint8_t i = 0U; i < TPMS_LAB_LIST_PAGE_SIZE; i++) {
        if(tpms_lab_saved_paths[i]) {
            furi_string_free(tpms_lab_saved_paths[i]);
            tpms_lab_saved_paths[i] = NULL;
        }
        tpms_lab_session_slots[i] = 0U;
    }
    tpms_lab_list_count = 0U;
}

static void tpms_lab_add_label(
    TPMSApp* app,
    const TpmsEditValues* values,
    const char* user_label,
    uint8_t local_index) {
    char label[64];
    if(user_label && user_label[0]) {
        snprintf(
            label,
            sizeof(label),
            "%s %08lX",
            user_label,
            (unsigned long)values->id);
    } else {
        snprintf(
            label,
            sizeof(label),
            "%08lX %.2fbar %s",
            (unsigned long)values->id,
            (double)(values->pressure_kpa / 100.0f),
            tpms_lab_sensor_protocol_short(values->kind));
    }
    submenu_add_item(app->submenu, label, local_index, tpms_lab_sensors_callback, app);
}

static uint16_t tpms_lab_build_session_page(TPMSApp* app) {
    uint16_t total = 0U;
    const uint16_t page_start = tpms_lab_list_page * TPMS_LAB_LIST_PAGE_SIZE;
    const uint16_t page_end = page_start + TPMS_LAB_LIST_PAGE_SIZE;
    for(uint8_t slot = 0U; slot < TPMS_LAB_MAX_SENSORS; slot++) {
        TpmsEditValues values = {0};
        char protocol[48] = {0};
        if(!tpms_lab_session_load(slot, &values, protocol, sizeof(protocol))) continue;
        const uint16_t global_index = total++;
        if(global_index >= page_start && global_index < page_end &&
           tpms_lab_list_count < TPMS_LAB_LIST_PAGE_SIZE) {
            tpms_lab_session_slots[tpms_lab_list_count] = slot;
            tpms_lab_add_label(app, &values, NULL, tpms_lab_list_count);
            tpms_lab_list_count++;
        }
    }
    return total;
}

static uint16_t tpms_lab_build_saved_page(TPMSApp* app) {
    tpms_lab_saved_migrate_legacy();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return 0U;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/tpms_editor"));
    storage_common_mkdir(storage, TPMS_LAB_SAVED_FOLDER);

    File* dir = storage_file_alloc(storage);
    uint16_t total = 0U;
    if(dir && storage_dir_open(dir, TPMS_LAB_SAVED_FOLDER)) {
        FileInfo info;
        char name[96];
        const uint32_t page_start = (uint32_t)tpms_lab_list_page * TPMS_LAB_LIST_PAGE_SIZE;
        const uint32_t page_end = page_start + TPMS_LAB_LIST_PAGE_SIZE;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info) || !tpms_lab_has_tpl_extension(name)) continue;
            const uint32_t global_index = total++;
            if(global_index < page_start || global_index >= page_end ||
               tpms_lab_list_count >= TPMS_LAB_LIST_PAGE_SIZE) continue;

            FuriString* path = furi_string_alloc();
            if(!path) continue;
            furi_string_printf(path, "%s/%s", TPMS_LAB_SAVED_FOLDER, name);

            TpmsEditValues values = {0};
            char protocol[48] = {0};
            if(!tpms_profile_load(
                   furi_string_get_cstr(path), &values, protocol, sizeof(protocol))) {
                furi_string_free(path);
                continue;
            }
            char user_label[32] = {0};
            uint32_t label_id = 0U;
            (void)tpms_profile_read_label(
                furi_string_get_cstr(path), user_label, sizeof(user_label), &label_id);
            UNUSED(label_id);
            tpms_lab_saved_paths[tpms_lab_list_count] = path;
            tpms_lab_add_label(app, &values, user_label, tpms_lab_list_count);
            tpms_lab_list_count++;
        }
        storage_dir_close(dir);
    }
    if(dir) storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return total;
}

static void tpms_lab_build_list(TPMSApp* app) {
    tpms_lab_list_clear();
    submenu_reset(app->submenu);

    uint16_t total = app->lab_sensor_list_source == TPMSLabListSession ?
                         tpms_lab_build_session_page(app) :
                         tpms_lab_build_saved_page(app);
    if(total > 0U && (uint32_t)tpms_lab_list_page * TPMS_LAB_LIST_PAGE_SIZE >= total &&
       tpms_lab_list_page > 0U) {
        tpms_lab_list_page = (uint16_t)((total - 1U) / TPMS_LAB_LIST_PAGE_SIZE);
        tpms_lab_list_clear();
        submenu_reset(app->submenu);
        total = app->lab_sensor_list_source == TPMSLabListSession ?
                    tpms_lab_build_session_page(app) :
                    tpms_lab_build_saved_page(app);
    }
    tpms_lab_list_total = total;

    if(total == 0U) {
        tpms_lab_list_page = 0U;
        submenu_set_header(
            app->submenu,
            app->lab_sensor_list_source == TPMSLabListSession ?
                "None in this session" : "No saved IDs");
    } else {
        const uint16_t pages = (uint16_t)((total + TPMS_LAB_LIST_PAGE_SIZE - 1U) /
                                          TPMS_LAB_LIST_PAGE_SIZE);
        char header[32];
        snprintf(
            header,
            sizeof(header),
            "%s %u/%u",
            app->lab_sensor_list_source == TPMSLabListSession ? "Session" : "Saved",
            (unsigned)(tpms_lab_list_page + 1U),
            (unsigned)pages);
        submenu_set_header(app->submenu, header);
        if(tpms_lab_list_page > 0U)
            submenu_add_item(app->submenu, "< Previous", TPMS_LAB_PREVIOUS, tpms_lab_sensors_callback, app);
        if(tpms_lab_list_page + 1U < pages)
            submenu_add_item(app->submenu, "Next >", TPMS_LAB_NEXT, tpms_lab_sensors_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewSubmenu);
}

void tpms_scene_lab_sensors_on_enter(void* context) {
    TPMSApp* app = context;
    if(!app || !app->submenu) return;
    if(app->txrx && app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);
    tpms_lab_list_page = 0U;
    tpms_lab_build_list(app);
}

bool tpms_scene_lab_sensors_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, TPMSSceneLabPressure);
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == TPMS_LAB_PREVIOUS) {
        if(tpms_lab_list_page > 0U) tpms_lab_list_page--;
        tpms_lab_build_list(app);
        return true;
    }
    if(event.event == TPMS_LAB_NEXT) {
        const uint32_t next = ((uint32_t)tpms_lab_list_page + 1U) * TPMS_LAB_LIST_PAGE_SIZE;
        if(next < tpms_lab_list_total) tpms_lab_list_page++;
        tpms_lab_build_list(app);
        return true;
    }
    if(event.event >= tpms_lab_list_count) return false;

    memset(&app->simulation_prefill_values, 0, sizeof(app->simulation_prefill_values));
    app->simulation_lab_path[0] = '\0';
    char protocol[48] = {0};
    bool ok = false;
    if(app->lab_sensor_list_source == TPMSLabListSession) {
        const uint8_t slot = tpms_lab_session_slots[event.event];
        ok = tpms_lab_session_load(
            slot, &app->simulation_prefill_values, protocol, sizeof(protocol));
        if(ok) {
            app->simulation_lab_source = TPMSLabSimulationSession;
            app->simulation_lab_session_slot = slot;
        }
    } else if(tpms_lab_saved_paths[event.event]) {
        snprintf(
            app->simulation_lab_path,
            sizeof(app->simulation_lab_path),
            "%s",
            furi_string_get_cstr(tpms_lab_saved_paths[event.event]));
        ok = tpms_profile_load(
            app->simulation_lab_path,
            &app->simulation_prefill_values,
            protocol,
            sizeof(protocol));
        if(ok) app->simulation_lab_source = TPMSLabSimulationSaved;
    }

    if(!ok) {
        app->simulation_lab_source = TPMSLabSimulationNone;
        notification_message(app->notifications, &sequence_blink_red_10);
        return true;
    }
    app->simulation_prefill_valid = true;
    scene_manager_next_scene(app->scene_manager, TPMSSceneSimulation);
    return true;
}

void tpms_scene_lab_sensors_on_exit(void* context) {
    TPMSApp* app = context;
    if(app && app->submenu) submenu_reset(app->submenu);
    tpms_lab_list_clear();
}
