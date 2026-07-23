#include "../trident_i.h"

typedef enum {
    Nrf24Spectrum,
    Nrf24Finder,
    Nrf24Sniffer,
} Nrf24Index;

static void trident_scene_nrf24_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_nrf24_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "NRF24 - 2.4 GHz");
    submenu_add_item(menu, "Spectrum Analyzer", Nrf24Spectrum, trident_scene_nrf24_cb, app);
    submenu_add_item(menu, "Channel Finder", Nrf24Finder, trident_scene_nrf24_cb, app);
    submenu_add_item(menu, "Sniffer (experimental)", Nrf24Sniffer, trident_scene_nrf24_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneNrf24));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_nrf24_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneNrf24, event.event);
        consumed = true;
        switch(event.event) {
        case Nrf24Spectrum:
            scene_manager_next_scene(app->scene_manager, TridentSceneNrf24scan);
            break;
        case Nrf24Finder:
            scene_manager_next_scene(app->scene_manager, TridentSceneNrf24find);
            break;
        case Nrf24Sniffer:
            scene_manager_next_scene(app->scene_manager, TridentSceneNrf24sniff);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_nrf24_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
