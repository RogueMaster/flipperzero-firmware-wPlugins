#include "../hotspot_arcade_i.h"
#include "../helpers/ha_storage.h"

static void ha_ssid_input_cb(void* context) {
    HotspotArcadeApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, HaEventSsidDone);
}

void hotspot_arcade_scene_ssid_input_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    strncpy(app->ssid_buf, furi_string_get_cstr(app->ssid), HA_SSID_MAX - 1);
    app->ssid_buf[HA_SSID_MAX - 1] = '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Hotspot name (SSID)");
    text_input_set_result_callback(
        app->text_input, ha_ssid_input_cb, app, app->ssid_buf, HA_SSID_MAX, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewTextInput);
}

bool hotspot_arcade_scene_ssid_input_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == HaEventSsidDone) {
        if(strlen(app->ssid_buf) > 0) {
            furi_string_set(app->ssid, app->ssid_buf);
            ha_storage_save_config(app);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void hotspot_arcade_scene_ssid_input_on_exit(void* context) {
    UNUSED(context);
}
