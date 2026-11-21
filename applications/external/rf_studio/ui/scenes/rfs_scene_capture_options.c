#include "../../rf_studio_i.h"

enum {
    OptionAuto,
    OptionFrequency,
    OptionPreset,
    OptionRadio,
    OptionRemoteFrequency,
    OptionExisting,
};

static void rfs_capture_options_label(RfStudioApp* app, uint32_t item, FuriString* out) {
    char mhz[16];
    switch(item) {
    case OptionFrequency:
        if(app->capture_frequency) {
            rfs_format_mhz(app->capture_frequency, mhz, sizeof(mhz));
            furi_string_printf(out, "Frequency: %s", mhz);
        } else {
            furi_string_set(out, "Frequency: Auto");
        }
        break;
    case OptionPreset:
        furi_string_printf(out, "Modulation: %s", app->capture_preset);
        break;
    case OptionRadio:
        furi_string_printf(out, "Radio: %s", rfs_radio_mode_name(app->capture_radio));
        break;
    case OptionRemoteFrequency:
        rfs_format_mhz(app->remote.preferred_frequency, mhz, sizeof(mhz));
        furi_string_printf(out, "Use Remote's %s", mhz);
        break;
    default:
        break;
    }
}

void rfs_scene_capture_options_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Capture Options");
    FuriString* label = furi_string_alloc();

    submenu_add_item(submenu, "Detect Frequency", OptionAuto, rfs_app_submenu_callback, app);
    const uint32_t labelled[] = {OptionFrequency, OptionPreset, OptionRadio};
    for(size_t i = 0; i < COUNT_OF(labelled); i++) {
        rfs_capture_options_label(app, labelled[i], label);
        submenu_add_item(
            submenu, furi_string_get_cstr(label), labelled[i], rfs_app_submenu_callback, app);
    }
    bool for_button = app->capture_target != RfsCaptureForLibrary;
    if(for_button && app->remote.preferred_frequency) {
        rfs_capture_options_label(app, OptionRemoteFrequency, label);
        submenu_add_item(
            submenu,
            furi_string_get_cstr(label),
            OptionRemoteFrequency,
            rfs_app_submenu_callback,
            app);
    }
    if(for_button) {
        /* CAP-ADD-005: pick a signal already in the library instead of capturing. */
        submenu_add_item(
            submenu, "Use Existing Signal", OptionExisting, rfs_app_submenu_callback, app);
    }
    furi_string_free(label);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RfsSceneCaptureOptions));
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_capture_options_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    uint32_t index = event.event - RfsEventMenuBase;
    scene_manager_set_scene_state(app->scene_manager, RfsSceneCaptureOptions, index);

    switch(index) {
    case OptionAuto:
        app->capture_frequency = 0;
        scene_manager_previous_scene(app->scene_manager);
        break;
    case OptionFrequency:
        scene_manager_next_scene(app->scene_manager, RfsSceneCaptureFreq);
        break;
    case OptionPreset:
        scene_manager_next_scene(app->scene_manager, RfsSceneCapturePreset);
        break;
    case OptionRadio: {
        app->capture_radio = (app->capture_radio + 1) % RfsRadioModeCount;
        FuriString* label = furi_string_alloc();
        rfs_capture_options_label(app, OptionRadio, label);
        submenu_change_item_label(app->submenu, OptionRadio, furi_string_get_cstr(label));
        furi_string_free(label);
        break;
    }
    case OptionRemoteFrequency:
        app->capture_frequency = app->remote.preferred_frequency;
        if(app->remote.preferred_preset[0]) {
            strlcpy(
                app->capture_preset, app->remote.preferred_preset, sizeof(app->capture_preset));
        }
        scene_manager_previous_scene(app->scene_manager);
        break;
    case OptionExisting:
        app->signal_list_mode = app->capture_target == RfsCaptureForRerecord ?
                                    RfsSignalListPickForButton :
                                    RfsSignalListPickNewButton;
        scene_manager_set_scene_state(app->scene_manager, RfsSceneSignalList, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneSignalList);
        break;
    default:
        break;
    }
    return true;
}

void rfs_scene_capture_options_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
