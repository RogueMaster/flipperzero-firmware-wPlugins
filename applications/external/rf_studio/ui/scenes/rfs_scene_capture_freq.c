#include "../../rf_studio_i.h"

/* Manual frequency selection (CAP-ADD-004). The list comes from the firmware settings and is
 * filtered only by what the selected radio reports it can receive. */

void rfs_scene_capture_freq_on_enter(void* context) {
    RfStudioApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Frequency (MHz)");

    RfsDeviceId id = RfsDeviceInternal;
    rfs_radio_resolve(app->radio, app->capture_radio, 0, false, &id, NULL);
    const SubGhzDevice* device = rfs_radio_device(app->radio, id);

    size_t count = rfs_radio_freq_count(app->radio, RfsDiscoverAll);
    uint32_t selected = 0;
    char label[24];
    for(size_t i = 0; i < count; i++) {
        uint32_t frequency = rfs_radio_freq_get(app->radio, RfsDiscoverAll, i);
        if(device && !subghz_devices_is_frequency_valid(device, frequency)) continue;
        rfs_format_mhz(frequency, label, sizeof(label));
        submenu_add_item(submenu, label, i, rfs_app_submenu_callback, app);
        if(frequency == app->capture_frequency) selected = i;
    }
    submenu_set_selected_item(submenu, selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewSubmenu);
}

bool rfs_scene_capture_freq_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event < RfsEventMenuBase) return false;
    size_t index = event.event - RfsEventMenuBase;
    app->capture_frequency = rfs_radio_freq_get(app->radio, RfsDiscoverAll, index);
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, RfsSceneCapture);
    return true;
}

void rfs_scene_capture_freq_on_exit(void* context) {
    RfStudioApp* app = context;
    submenu_reset(app->submenu);
}
