#include "../app_user.h"
#include "../modules/lldp_module.h"
#include "../modules/passive_discovery_module.h"
#include "../libraries/protocol_tools/neighbor_db.h"

static const char* passive_protocol_names[] = {
    "ALL",
    "LLDP",
    "EAPOL",
    "CDP",
};

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context);

static void passive_discovery_draw_config(App* app) {
    widget_reset(app->widget);

    char protocol_text[32];

    uint8_t protocol = app->passive_discovery.protocol;

#if DEV_MODE

    if(protocol >= 4) {
        protocol = PassiveProtocolALL;
        app->passive_discovery.protocol = protocol;
    }

#else

    // Release: solamente LLDP visible
    if(protocol != PassiveProtocolLLDP) {
        protocol = PassiveProtocolLLDP;
        app->passive_discovery.protocol = protocol;
    }

#endif

    snprintf(protocol_text, sizeof(protocol_text), "%s", passive_protocol_names[protocol]);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Protocol");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, protocol_text);

    /*widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "<",
        passive_discovery_button_callback,
        app);*/

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Start", passive_discovery_button_callback, app);

    /*widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        ">",
        passive_discovery_button_callback,
        app);*/
}

static void passive_discovery_draw_listening(App* app) {
    char neighbors_text[32];

    snprintf(neighbors_text, sizeof(neighbors_text), "Neighbors: %u", app->passive_neighbor_count);

    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Listening");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, neighbors_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Stop", passive_discovery_button_callback, app);
}

static void passive_discovery_refresh(App* app) {
    switch(app->passive_discovery.state) {
    case PassiveDiscoveryStateConfig:

        passive_discovery_draw_config(app);
        break;

    case PassiveDiscoveryStateListening:

        passive_discovery_draw_listening(app);
        break;

    case PassiveDiscoveryStateFinished:

        app->passive_selected_neighbor = 0;

        scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_list_option);

        break;

    default:
        break;
    }
}

void app_scene_passive_discovery_on_enter(void* context) {
    App* app = context;

    if(!app) {
        return;
    }

    neighbor_db_load();

    app->passive_discovery.state = PassiveDiscoveryStateConfig;

    app->passive_discovery.protocol = PassiveProtocolALL;

    app->passive_discovery_stop = false;
    app->thread_alternative = NULL;

    app->passive_neighbor_count = neighbor_db_count();

    passive_discovery_refresh(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_passive_discovery_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            passive_discovery_module_stop(app);

            app->passive_discovery.state = PassiveDiscoveryStateConfig;

            passive_discovery_refresh(app);

            return true;
        }

        return scene_manager_previous_scene(app->scene_manager);
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            passive_discovery_refresh(app);

            return true;
        }
    }

    return false;
}

void app_scene_passive_discovery_on_exit(void* context) {
    App* app = context;

    if(!app) {
        return;
    }

    app->passive_discovery_stop = true;

    passive_discovery_module_stop(app);

    widget_reset(app->widget);
}

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;

    if(input_type != InputTypeShort) {
        return;
    }

    switch(type) {
    case GuiButtonTypeLeft:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
#if DEV_MODE

            if(app->passive_discovery.protocol == PassiveProtocolALL) {
                app->passive_discovery.protocol = PassiveProtocolCDP;

            } else {
                app->passive_discovery.protocol--;
            }

#else

            app->passive_discovery.protocol = PassiveProtocolLLDP;

#endif

            passive_discovery_refresh(app);
        }

        break;

    case GuiButtonTypeRight:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
#if DEV_MODE

            app->passive_discovery.protocol++;

            if(app->passive_discovery.protocol >= 4) {
                app->passive_discovery.protocol = PassiveProtocolALL;
            }

#else

            app->passive_discovery.protocol = PassiveProtocolLLDP;

#endif

            passive_discovery_refresh(app);
        }

        break;

    case GuiButtonTypeCenter:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            app->passive_discovery.state = PassiveDiscoveryStateListening;

            passive_discovery_refresh(app);

            passive_discovery_module_start(app);

        } else if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            passive_discovery_module_stop(app);

            app->passive_discovery.state = PassiveDiscoveryStateFinished;

            passive_discovery_refresh(app);
        }

        break;

    default:
        break;
    }
}
