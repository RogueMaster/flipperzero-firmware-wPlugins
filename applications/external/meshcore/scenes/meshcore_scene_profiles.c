/*
 * scene_profiles — pick a radio preset to push onto the node.
 *
 * The list is the built-ins plus whatever is on the card. Entries that failed
 * to load are listed too, with a marker: a preset file that silently vanishes
 * looks exactly like one that was never written, and the first thing anyone
 * does with a hand-edited JSON file is mistype a field name.
 *
 * Selecting an entry hands over to scene_apply, which is where anything is
 * actually sent.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_PROFILES_EVENT_LOADED 0x400u
#define MESHCORE_PROFILES_WORKER_STACK 2048u

/* Above any entry index, so the two cannot be confused in the submenu. */
#define MESHCORE_PROFILES_INDEX_SAVE 0x100u

static void meshcore_scene_profiles_callback(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/* Reading the card blocks, so it happens off the GUI thread. */
static int32_t meshcore_scene_profiles_worker(void* context) {
    MeshCoreApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    meshcore_preset_store_scan(&app->presets, storage);
    furi_record_close(RECORD_STORAGE);

    /* Said out loud, per entry: this is the one place a hand-edited file gets
     * judged, and "it did not show up" is not a diagnosis. */
    meshcore_log_printf(
        app->log,
        "presets: %u entries, %lu skipped",
        (unsigned)app->presets.count,
        (unsigned long)app->presets.skipped);
    for(size_t i = 0; i < app->presets.count; i++) {
        const MeshCorePresetEntry* entry = &app->presets.items[i];
        if(meshcore_preset_store_usable(entry)) {
            meshcore_log_printf(
                app->log,
                "  %.20s %lu kHz sf%u",
                entry->preset.name,
                (unsigned long)entry->preset.freq_khz,
                (unsigned)entry->preset.sf);
        } else {
            meshcore_log_printf(app->log, "  %.16s FAILED: %.39s", entry->file, entry->error);
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_PROFILES_EVENT_LOADED);
    return 0;
}

static void meshcore_scene_profiles_show(MeshCoreApp* app) {
    Submenu* submenu = app->submenu;
    char label[40];

    submenu_reset(submenu);
    submenu_set_header(submenu, "Profiles");

    for(size_t i = 0; i < app->presets.count; i++) {
        const MeshCorePresetEntry* entry = &app->presets.items[i];
        if(meshcore_preset_store_usable(entry)) {
            char freq[MESHCORE_PRESET_FIELD_LEN];
            meshcore_preset_format_freq(entry->preset.freq_khz, freq, sizeof(freq));
            snprintf(label, sizeof(label), "%.20s  %s", entry->preset.name, freq);
        } else {
            /* Shown, not hidden — and marked, so the reason can be read on the
             * next screen rather than guessed at. */
            snprintf(label, sizeof(label), "! %.30s", entry->preset.name);
        }
        submenu_add_item(submenu, label, (uint32_t)i, meshcore_scene_profiles_callback, app);
    }

    /* Cloning is the point of the whole feature: set one node up by hand, then
     * put the same numbers on the rest without retyping five fields. */
    if(app->node.valid) {
        submenu_add_item(
            submenu,
            "Save from node",
            MESHCORE_PROFILES_INDEX_SAVE,
            meshcore_scene_profiles_callback,
            app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneProfiles));
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSubmenu);
}

/* Turn what the node is doing right now into a preset. */
static void meshcore_scene_profiles_save(MeshCoreApp* app) {
    MeshCorePreset preset;
    memset(&preset, 0, sizeof(preset));

    snprintf(preset.name, sizeof(preset.name), "%.23s", app->node.name);
    preset.freq_khz = app->node.freq_khz;
    preset.bw_hz = app->node.bw_hz;
    preset.sf = app->node.sf;
    preset.cr = app->node.cr;
    /* SELF_INFO does not carry it, so the file is written without one and the
     * loader defaults it to the firmware default rather than inventing a value
     * that would be silently pushed onto every other node. */
    preset.path_hash_bytes = 1;
    preset.has_node_name = false;

    const char* why = NULL;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = meshcore_preset_store_save(storage, &preset, &why);
    if(ok) meshcore_preset_store_scan(&app->presets, storage);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        meshcore_log_printf(app->log, "preset saved: %s", preset.name);
    } else {
        meshcore_log_printf(app->log, "preset save failed: %s", why ? why : "unknown");
    }
}

void meshcore_scene_profiles_on_enter(void* context) {
    MeshCoreApp* app = context;

    if(app->presets.scanned) {
        meshcore_scene_profiles_show(app);
        return;
    }

    /* First visit: show the built-ins immediately and fill in the card's
     * presets when the worker comes back. The list is never empty. */
    meshcore_scene_profiles_show(app);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreProfiles", MESHCORE_PROFILES_WORKER_STACK, meshcore_scene_profiles_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_profiles_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_PROFILES_EVENT_LOADED) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        meshcore_scene_profiles_show(app);
        return true;
    }

    if(event.event == MESHCORE_PROFILES_INDEX_SAVE) {
        meshcore_scene_profiles_save(app);
        meshcore_scene_profiles_show(app);
        return true;
    }

    if(event.event < app->presets.count) {
        scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneProfiles, event.event);
        app->preset_index = (size_t)event.event;
        scene_manager_next_scene(app->scene_manager, MeshCoreSceneApply);
        return true;
    }

    return false;
}

void meshcore_scene_profiles_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    submenu_reset(app->submenu);
}
