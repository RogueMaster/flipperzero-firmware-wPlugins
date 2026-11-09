/*
 * scene_settings — the three feedback toggles: Sound, Vibro, LED.
 *
 * Separate on purpose (the addition to stage 2 asked for it): on the river you
 * might want the LED only (silent but visible), vibro only (dark and quiet), or
 * everything off at night. Each change takes effect at once — the notifier reads
 * the settings live — and is saved to the card so it survives a restart.
 */
#include "../meshcore_cfg.h"

static const char* const meshcore_settings_onoff[] = {"Off", "On"};

/* Secret: mash one toggle Left-Right-Left-Right fast — six quick flips of the
 * SAME switch inside the window — to play the victory dance. Six is even, so the
 * switch lands back where it started. A casual re-configure never hits six fast
 * flips of one switch, and slowing down resets the count. Items are numbered by
 * add order: Sound 0, Vibro 1, LED 2 — mashing any one works. */
#define MESHCORE_SECRET_FLIPS     6
#define MESHCORE_SECRET_WINDOW_MS 500
static uint8_t meshcore_secret_item = 0xFF;
static uint8_t meshcore_secret_count;
static uint32_t meshcore_secret_tick;

static void meshcore_settings_secret(MeshCoreApp* app, uint8_t item) {
    uint32_t now = furi_get_tick();
    if(item == meshcore_secret_item &&
       (now - meshcore_secret_tick) <= furi_ms_to_ticks(MESHCORE_SECRET_WINDOW_MS)) {
        meshcore_secret_count++;
    } else {
        meshcore_secret_item = item;
        meshcore_secret_count = 1;
    }
    meshcore_secret_tick = now;
    if(meshcore_secret_count >= MESHCORE_SECRET_FLIPS) {
        /* play_song blocks while it plays — that IS the show. Reset so one more
         * flip cannot immediately re-trigger it. */
        meshcore_secret_count = 0;
        meshcore_secret_item = 0xFF;
        meshcore_notify_play_song(app->notify);
    }
}

static void meshcore_settings_sound_changed(VariableItem* item) {
    MeshCoreApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sound = idx != 0;
    variable_item_set_current_value_text(item, meshcore_settings_onoff[idx]);
    meshcore_settings_save(&app->settings);
    meshcore_settings_secret(app, 0);
}

static void meshcore_settings_vibro_changed(VariableItem* item) {
    MeshCoreApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.vibro = idx != 0;
    variable_item_set_current_value_text(item, meshcore_settings_onoff[idx]);
    meshcore_settings_save(&app->settings);
    meshcore_settings_secret(app, 1);
}

static void meshcore_settings_led_changed(VariableItem* item) {
    MeshCoreApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.led = idx != 0;
    variable_item_set_current_value_text(item, meshcore_settings_onoff[idx]);
    meshcore_settings_save(&app->settings);
    /* Turning the LED off should clear any held status colour right away, not
     * wait for the next logger reading. */
    if(!app->settings.led) meshcore_notify_led_off(app->notify);
    meshcore_settings_secret(app, 2);
}

static void meshcore_settings_add(
    MeshCoreApp* app,
    const char* label,
    bool on,
    VariableItemChangeCallback cb) {
    VariableItem* item = variable_item_list_add(app->var_list, label, 2, cb, app);
    variable_item_set_current_value_index(item, on ? 1 : 0);
    variable_item_set_current_value_text(item, meshcore_settings_onoff[on ? 1 : 0]);
}

void meshcore_scene_settings_on_enter(void* context) {
    MeshCoreApp* app = context;

    /* Start the secret fresh each visit. */
    meshcore_secret_item = 0xFF;
    meshcore_secret_count = 0;

    variable_item_list_reset(app->var_list);
    meshcore_settings_add(app, "Sound", app->settings.sound, meshcore_settings_sound_changed);
    meshcore_settings_add(app, "Vibro", app->settings.vibro, meshcore_settings_vibro_changed);
    meshcore_settings_add(app, "LED", app->settings.led, meshcore_settings_led_changed);

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewVarList);
}

bool meshcore_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void meshcore_scene_settings_on_exit(void* context) {
    MeshCoreApp* app = context;
    variable_item_list_reset(app->var_list);
}
