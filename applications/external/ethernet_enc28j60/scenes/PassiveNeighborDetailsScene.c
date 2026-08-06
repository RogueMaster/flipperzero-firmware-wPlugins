#include "../app_user.h"
#include "../modules/passive_discovery_module.h"
#include "../libraries/protocol_tools/neighbor_db.h"

static void passive_details_callback(GuiButtonType type, InputType input_type, void* context);

static void mac_to_string(uint8_t* mac, char* buffer, size_t size) {
    snprintf(
        buffer,
        size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

static void passive_format_sources(const neighbor_t* neighbor, char* output, size_t size) {
    if(!neighbor || !output) {
        return;
    }

    bool lldp = neighbor->discovery_sources & NEIGHBOR_SOURCE_LLDP;

    bool cdp = neighbor->discovery_sources & NEIGHBOR_SOURCE_CDP;

    bool eap = neighbor->discovery_sources & NEIGHBOR_SOURCE_EAPOL;

    if(lldp && cdp && eap) {
        snprintf(output, size, "LLDP CDP EAP");

    } else if(lldp && cdp) {
        snprintf(output, size, "LLDP CDP");

    } else if(lldp && eap) {
        snprintf(output, size, "LLDP EAP");

    } else if(cdp && eap) {
        snprintf(output, size, "CDP EAP");

    } else if(lldp) {
        snprintf(output, size, "LLDP");

    } else if(cdp) {
        snprintf(output, size, "CDP");

    } else if(eap) {
        snprintf(output, size, "EAPOL");

    } else {
        snprintf(output, size, "Unknown");
    }
}

static void passive_draw_details(App* app, neighbor_t* neighbor) {
    widget_reset(app->widget);

    char mac_str[20];
    char source_str[32];

    if(!neighbor) {
        return;
    }

    if(app->passive_details_page > 0) {
        char line1[64];
        char line2[64];
        char line3[64];
        char line4[64];

        passive_discovery_module_build_details_page(
            PassiveProtocolLLDP,
            neighbor,
            app->passive_details_page - 1,
            line1,
            sizeof(line1),
            line2,
            sizeof(line2),
            line3,
            sizeof(line3),
            line4,
            sizeof(line4));

        widget_add_string_element(
            app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, line1);

        widget_add_string_element(
            app->widget, 64, 23, AlignCenter, AlignCenter, FontSecondary, line2);

        widget_add_string_element(
            app->widget, 64, 38, AlignCenter, AlignCenter, FontSecondary, line3);

        widget_add_string_element(
            app->widget, 64, 53, AlignCenter, AlignCenter, FontSecondary, line4);

        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "<", passive_details_callback, app);

        widget_add_button_element(
            app->widget, GuiButtonTypeRight, ">", passive_details_callback, app);

        return;
    }

    mac_to_string(neighbor->mac, mac_str, sizeof(mac_str));

    passive_format_sources(neighbor, source_str, sizeof(source_str));

    widget_add_string_element(
        app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "NEIGHBOR");

    widget_add_string_element(
        app->widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        neighbor->name[0] ? neighbor->name : "Unknown");

    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, mac_str);

    widget_add_string_element(
        app->widget,
        64,
        43,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        neighbor->port[0] ? neighbor->port : "No Port ID");

    widget_add_string_element(
        app->widget, 64, 54, AlignCenter, AlignCenter, FontSecondary, source_str);

    widget_add_button_element(app->widget, GuiButtonTypeLeft, "<", passive_details_callback, app);

    widget_add_button_element(app->widget, GuiButtonTypeRight, ">", passive_details_callback, app);
}

void app_scene_passive_neighbor_details_on_enter(void* context) {
    App* app = context;
    app->passive_details_page = 0;

    neighbor_t* neighbor = neighbor_db_get_by_position(app->passive_selected_neighbor);

    if(!neighbor) {
        widget_reset(app->widget);

        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "Neighbor lost");

        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

        return;
    }

    passive_draw_details(app, neighbor);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void passive_details_callback(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;

    if(input_type != InputTypeShort) {
        return;
    }

    neighbor_t* neighbor = neighbor_db_get_by_position(app->passive_selected_neighbor);

    if(!neighbor) {
        return;
    }

    if(type == GuiButtonTypeRight) {
        app->passive_details_page++;

        /*
         * Por ahora dejamos 3 páginas LLDP.
         * Después lo hacemos dinámico con
         * passive_discovery_module_get_details_page_count()
         */

        if(app->passive_details_page >= 4) {
            app->passive_details_page = 0;
        }

        passive_draw_details(app, neighbor);

    } else if(type == GuiButtonTypeLeft) {
        if(app->passive_details_page > 0) {
            app->passive_details_page--;

            passive_draw_details(app, neighbor);

        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
    }
}

bool app_scene_passive_neighbor_details_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void app_scene_passive_neighbor_details_on_exit(void* context) {
    App* app = context;

    widget_reset(app->widget);
}
