#include "led_remote.h"
#include "helpers/led_remote_storage.h"
#include "helpers/led_remote_ir.h"
#include <string.h>

// ─── 18-keys data (3 columns × 6 rows) ───────────────────────────────────────
static const char* const BTN_NAMES_18K[18] = {
    "On",      "Off",    "Timer",
    "Red",     "Green",  "Blue",
    "Orange",  "Lime",   "Cyan",
    "Yellow",  "Multi",  "Purple",
    "Bright+", "Music",  "Bright-",
    "Strobe",  "Fade",   "Flash",
};
static const char* const BTN_LABELS_18K[18] = {
    "ON", "OF", "5m",
    "R",  "G",  "B",
    "O",  "LG", "Cy",
    "Y",  "Ml", "Pu",
    "+",  "Mu", "-",
    "ST", "FD", "FL",
};

// ─── 24-keys data (4 columns × 6 rows) ───────────────────────────────────────
static const char* const BTN_NAMES_24K[24] = {
    "Bright+",    "Bright-",   "Off",      "On",
    "Red",        "Green",     "Blue",     "White",
    "Orange-Red", "Lime",      "Sky Blue", "Flash",
    "Orange",     "Cyan",      "Purple",   "Strobe",
    "Amber",      "Teal",      "DkPurple", "Fade",
    "Yellow",     "Indigo",    "Pink",     "Smooth",
};
static const char* const BTN_LABELS_24K[24] = {
    "B+", "B-", "OF", "ON",
    "R",  "G",  "B",  "W",
    "R+", "LM", "SB", "FL",
    "OR", "CY", "PU", "ST",
    "AM", "TE", "DP", "FD",
    "YL", "IN", "PK", "SM",
};

// ─── Type definitions ─────────────────────────────────────────────────────────

const LedRemoteTypeDef LED_REMOTE_TYPE_DEFS[LedRemoteTypeCount] = {
    [LedRemoteType18Keys] = {
        .button_count        = 18,
        .grid_cols           = 3,
        .type_name           = "18-Keys Remote",
        .file_prefix         = "18k_",
        .button_names        = BTN_NAMES_18K,
        .button_labels_short = BTN_LABELS_18K,
    },
    [LedRemoteType24Keys] = {
        .button_count        = 24,
        .grid_cols           = 4,
        .type_name           = "24-Keys Remote",
        .file_prefix         = "24k_",
        .button_names        = BTN_NAMES_24K,
        .button_labels_short = BTN_LABELS_24K,
    },
};

// ─── File path ────────────────────────────────────────────────────────────────

// Builds the .ir path for any (type, profile_name) pair
static void build_ir_path(
    LedRemoteType type,
    const char* profile_name,
    char* out,
    size_t out_size) {
    char safe[32] = {0};
    strncpy(safe, profile_name, sizeof(safe) - 1);
    for(size_t i = 0; safe[i]; i++) {
        if(safe[i] == ' ') safe[i] = '_';
    }
    const char* prefix = LED_REMOTE_TYPE_DEFS[type].file_prefix;
    snprintf(out, out_size, LED_REMOTE_DATA_DIR "/%s%s.ir", prefix, safe);
}

void led_remote_build_save_path(LedRemoteApp* app) {
    build_ir_path(app->remote_type, app->profile_name, app->save_path, sizeof(app->save_path));
}

// ─── Remote type ──────────────────────────────────────────────────────────────

void led_remote_apply_type(LedRemoteApp* app) {
    const LedRemoteTypeDef* td = &LED_REMOTE_TYPE_DEFS[app->remote_type];
    app->button_count        = td->button_count;
    app->grid_cols           = td->grid_cols;
    app->button_names        = td->button_names;
    app->button_labels_short = td->button_labels_short;
}

void led_remote_switch_type(LedRemoteApp* app, LedRemoteType new_type) {
    if(app->remote_type == new_type) return;

    led_remote_storage_save(app);

    app->remote_type = new_type;
    led_remote_apply_type(app);
    led_remote_build_save_path(app);
    led_remote_settings_save(app);

    for(size_t i = 0; i < LED_REMOTE_MAX_BUTTONS; i++) {
        led_remote_ir_signal_clear(&app->signals[i]);
        app->learned[i] = false;
    }

    led_remote_storage_load(app);
}

// ─── Profiles ─────────────────────────────────────────────────────────────────

static void switch_profile_internal(LedRemoteApp* app, const char* name, bool save_current) {
    if(strncmp(app->profile_name, name, sizeof(app->profile_name) - 1) == 0) return;

    if(save_current) led_remote_storage_save(app);

    strncpy(app->profile_name, name, sizeof(app->profile_name) - 1);
    app->profile_name[sizeof(app->profile_name) - 1] = '\0';
    led_remote_build_save_path(app);
    led_remote_settings_save(app);

    for(size_t i = 0; i < app->button_count; i++) {
        led_remote_ir_signal_clear(&app->signals[i]);
        app->learned[i] = false;
    }

    led_remote_storage_load(app);
}

void led_remote_switch_profile(LedRemoteApp* app, const char* name) {
    switch_profile_internal(app, name, true);
}

void led_remote_scan_profiles(LedRemoteApp* app) {
    app->profile_count = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, LED_REMOTE_DATA_DIR)) {
        const char* full_prefix = LED_REMOTE_TYPE_DEFS[app->remote_type].file_prefix;
        size_t prefix_len = strlen(full_prefix);

        FileInfo file_info;
        char entry[64];
        while(storage_dir_read(dir, &file_info, entry, sizeof(entry))) {
            size_t elen = strlen(entry);
            // Must start with the prefix and end with ".ir" (at least 1 char of name)
            if(elen < prefix_len + 4) continue;
            if(strncmp(entry, full_prefix, prefix_len) != 0) continue;
            if(strcmp(entry + elen - 3, ".ir") != 0) continue;

            // Extract the profile name (between the prefix and ".ir")
            size_t plen = elen - prefix_len - 3;
            if(plen == 0 || plen >= 32) continue;

            char* slot = app->profiles[app->profile_count];
            strncpy(slot, entry + prefix_len, plen);
            slot[plen] = '\0';
            // Convert underscores back to spaces
            for(size_t i = 0; slot[i]; i++) {
                if(slot[i] == '_') slot[i] = ' ';
            }

            app->profile_count++;
            if(app->profile_count >= LED_REMOTE_MAX_PROFILES) break;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    // The active profile must always appear in the list (skip if no active profile)
    if(app->profile_name[0] != '\0') {
        bool found = false;
        for(uint8_t i = 0; i < app->profile_count; i++) {
            if(strncmp(app->profiles[i], app->profile_name, sizeof(app->profiles[0]) - 1) == 0) {
                found = true;
                break;
            }
        }
        if(!found && app->profile_count < LED_REMOTE_MAX_PROFILES) {
            strncpy(
                app->profiles[app->profile_count],
                app->profile_name,
                sizeof(app->profiles[0]) - 1);
            app->profile_count++;
        }
    }
}

void led_remote_rename_profile(LedRemoteApp* app, const char* old_name, const char* new_name) {
    if(strlen(new_name) == 0) return;

    char old_path[128], new_path[128];
    build_ir_path(app->remote_type, old_name, old_path, sizeof(old_path));
    build_ir_path(app->remote_type, new_name, new_path, sizeof(new_path));

    storage_common_rename(app->storage, old_path, new_path);

    if(strncmp(app->profile_name, old_name, sizeof(app->profile_name) - 1) == 0) {
        strncpy(app->profile_name, new_name, sizeof(app->profile_name) - 1);
        app->profile_name[sizeof(app->profile_name) - 1] = '\0';
        led_remote_build_save_path(app);
        led_remote_settings_save(app);
    }
}

void led_remote_delete_profile(LedRemoteApp* app, const char* name) {
    char path[128];
    build_ir_path(app->remote_type, name, path, sizeof(path));
    storage_common_remove(app->storage, path);

    if(strncmp(app->profile_name, name, sizeof(app->profile_name) - 1) == 0) {
        app->profile_name[0] = '\0';
        led_remote_scan_profiles(app);
        for(uint8_t i = 0; i < app->profile_count; i++) {
            if(app->profiles[i][0] == '\0') continue;
            switch_profile_internal(app, app->profiles[i], false);
            return;
        }
        switch_profile_internal(app, "Remote 1", false);
    }
}

static bool led_remote_custom_event_callback(void* context, uint32_t event) {
    LedRemoteApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static LedRemoteApp* led_remote_app_alloc(void) {
    LedRemoteApp* app = malloc(sizeof(LedRemoteApp));
    memset(app, 0, sizeof(LedRemoteApp));

    app->scene_manager = scene_manager_alloc(&led_remote_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, led_remote_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, led_remote_scene_navigation_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LedRemoteViewIdSubmenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LedRemoteViewIdPopup, popup_get_view(app->popup));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LedRemoteViewIdTextInput,
        text_input_get_view(app->text_input));

    app->view_remote = led_remote_view_remote_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LedRemoteViewIdRemote,
        led_remote_view_remote_get_view(app->view_remote));

    app->ir_worker     = infrared_worker_alloc();
    app->storage       = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->remote_type = LedRemoteType18Keys;
    strncpy(app->profile_name, "Remote 1", sizeof(app->profile_name) - 1);
    app->layout = LedRemoteLayoutCircle;
    led_remote_settings_load(app);

    led_remote_apply_type(app);
    led_remote_build_save_path(app);

    led_remote_storage_load(app);

    return app;
}

static void led_remote_app_free(LedRemoteApp* app) {
    led_remote_storage_save(app);
    led_remote_settings_save(app);

    infrared_worker_free(app->ir_worker);
    furi_record_close(RECORD_NOTIFICATION);

    for(size_t i = 0; i < LED_REMOTE_MAX_BUTTONS; i++) {
        led_remote_ir_signal_clear(&app->signals[i]);
    }

    view_dispatcher_remove_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, LedRemoteViewIdRemote);
    view_dispatcher_remove_view(app->view_dispatcher, LedRemoteViewIdPopup);
    view_dispatcher_remove_view(app->view_dispatcher, LedRemoteViewIdTextInput);

    submenu_free(app->submenu);
    popup_free(app->popup);
    text_input_free(app->text_input);
    led_remote_view_remote_free(app->view_remote);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t led_remote_app(void* p) {
    UNUSED(p);

    LedRemoteApp* app = led_remote_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, LedRemoteSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    led_remote_app_free(app);

    return 0;
}
