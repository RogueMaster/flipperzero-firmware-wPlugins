
#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:T-Mobilitat"


static bool tmobilitat_parse(FuriString* parsed_data, Metroflip* app) {
    bool parsed = false;

    do {
        // Safety check: make sure there are at least 4 historical bytes
        if(app->hist_bytes_count < 4) {
            FURI_LOG_E(TAG, "Not enough historical bytes!");
            break;
        }

        // Take the last 4 bytes
        const uint8_t* last4 = app->hist_bytes + (app->hist_bytes_count - 4);

        // Convert to 32-bit unsigned integer (big-endian)
        uint32_t card_number_decimal = 
            (last4[0] << 24) |
            (last4[1] << 16) |
            (last4[2] << 8)  |
            (last4[3]);

        FURI_LOG_I(TAG, "Card number: %lu", (unsigned long)card_number_decimal);
        furi_string_printf(
                parsed_data,
                "\e#T-Mobilitat\nCard number: %lu",
                (unsigned long)card_number_decimal
            );
        parsed = true;
    } while(false);

    return parsed;
}

static void tmobilitat_on_enter(Metroflip* app) {
    FuriString* parsed_data = furi_string_alloc();
    Widget* widget = app->widget;

    furi_string_reset(app->text_box_store);
    widget_reset(widget);
    
    
    if(!tmobilitat_parse(parsed_data, app)) {
        furi_string_reset(app->text_box_store);
        FURI_LOG_I(TAG, "Unknown card type");
        furi_string_printf(parsed_data, "\e#Unknown card\n");
    }
    widget_add_text_scroll_element(
        widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

    widget_add_button_element(
        widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    furi_string_free(parsed_data);

}


static bool tmobilitat_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void tmobilitat_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    // Clear view
    popup_reset(app->popup);
    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin tmobilitat_plugin = {
    .card_name = "T-Mobilitat",
    .plugin_on_enter = tmobilitat_on_enter,
    .plugin_on_event = tmobilitat_on_event,
    .plugin_on_exit = tmobilitat_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor tmobilitat_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &tmobilitat_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* tmobilitat_plugin_ep(void) {
    return &tmobilitat_plugin_descriptor;
}
