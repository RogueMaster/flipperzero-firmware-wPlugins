#include "seos_scene_popup.h"

static void seos_scene_popup_timeout(void* context) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventViewExit);
}

void seos_scene_popup_enter(Seos* seos, const SeosScenePopup* config) {
    Popup* popup = seos->popup;
    if(config->icon) {
        popup_set_icon(popup, config->icon_x, config->icon_y, config->icon);
    }
    popup_set_header(
        popup,
        config->header,
        config->header_x,
        config->header_y,
        config->horizontal,
        config->vertical);
    if(config->text) {
        popup_set_text(
            popup,
            config->text,
            config->text_x,
            config->text_y,
            config->horizontal,
            config->vertical);
    }
    popup_set_timeout(popup, config->timeout_ms);
    popup_set_context(popup, seos);
    popup_set_callback(popup, seos_scene_popup_timeout);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
}

bool seos_scene_popup_event(Seos* seos, SceneManagerEvent event, uint32_t target_scene) {
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != SeosCustomEventViewExit) return false;

    return scene_manager_search_and_switch_to_previous_scene(seos->scene_manager, target_scene);
}

void seos_scene_popup_exit(Seos* seos) {
    popup_reset(seos->popup);
}
