#include "../app_user.h"
#include "../libraries/protocol_tools/neighbor_db.h"

static void passive_neighbor_callback(void* context, uint32_t index);
static void build_neighbor_submenu(App* app);

static uint16_t passive_scene_get_source(App* app) {
    switch(app->passive_discovery.protocol) {
    case PassiveProtocolALL:
        return NEIGHBOR_SOURCE_LLDP | NEIGHBOR_SOURCE_CDP | NEIGHBOR_SOURCE_EAPOL;

    case PassiveProtocolLLDP:
        return NEIGHBOR_SOURCE_LLDP;

    case PassiveProtocolCDP:
        return NEIGHBOR_SOURCE_CDP;

    case PassiveProtocolEAPOL:
        return NEIGHBOR_SOURCE_EAPOL;

    default:
        return 0;
    }
}

void app_scene_passive_neighbor_list_on_enter(void* context) {
    App* app = context;

    build_neighbor_submenu(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

static void build_neighbor_submenu(App* app) {
    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "DISCOVERED DEVICES");

    uint16_t source = passive_scene_get_source(app);

    size_t count;

    if(app->passive_discovery.protocol == PassiveProtocolALL) {
        count = neighbor_db_count();

    } else {
        count = neighbor_db_count_by_source(source);
    }

    if(count == 0) {
        submenu_add_item(app->submenu, "No neighbors", 0, passive_neighbor_callback, app);

    } else {
        for(size_t i = 0; i < count; i++) {
            neighbor_t* neighbor;

            if(app->passive_discovery.protocol == PassiveProtocolALL) {
                neighbor = neighbor_db_get(i);

            } else {
                neighbor = neighbor_db_get_by_source(source, i);
            }

            if(!neighbor) {
                continue;
            }

            char name[32];

            if(neighbor->name[0]) {
                snprintf(name, sizeof(name), "%.31s", neighbor->name);

            } else {
                snprintf(
                    name,
                    sizeof(name),
                    "%02X:%02X:%02X:%02X:%02X:%02X",
                    neighbor->mac[0],
                    neighbor->mac[1],
                    neighbor->mac[2],
                    neighbor->mac[3],
                    neighbor->mac[4],
                    neighbor->mac[5]);
            }

            submenu_add_item(app->submenu, name, i, passive_neighbor_callback, app);
        }
    }

    submenu_add_item(app->submenu, "[ Clear Results ]", 0xFFFF, passive_neighbor_callback, app);

    submenu_set_selected_item(app->submenu, app->passive_selected_neighbor);
}

static void passive_neighbor_callback(void* context, uint32_t index) {
    App* app = context;

    if(index == 0xFFFF) {
        neighbor_db_clear_by_source(passive_scene_get_source(app));

        neighbor_db_save();

        build_neighbor_submenu(app);

        return;
    }

    app->passive_selected_neighbor = index;

    scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_details_option);
}

bool app_scene_passive_neighbor_list_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void app_scene_passive_neighbor_list_on_exit(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
}
