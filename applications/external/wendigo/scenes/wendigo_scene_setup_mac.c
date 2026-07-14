#include "../wendigo_app_i.h"
#include <gui/modules/loading.h>
/* Included to have access to wendigo_log() - Seems excessive */
#include "../wendigo_scan.h"

/** Maximum length of an interface string ("WiFi", "Bluetooth", etc.) */
#define IF_MAX_LEN (10)

/** Local buffer for use of the view */
uint8_t view_bytes[MAC_BYTES];

/** Strings for popups */
char popup_header_text[IF_MAX_LEN + 11] = "";
char popup_text[IF_MAX_LEN + 50] = "";

/** State variables to keep track of interface and target MAC while updating */
InterfaceType updated_interface;
uint8_t updated_mac[MAC_BYTES];

/** Loading dialogue in case we need to wait to receive UART packets */
Loading* loading = NULL;

/** Callback invoked by the UART receiver thread when a MAC
 * packet is received and we're waiting for an updated MAC.
 */
void wendigo_scene_setup_mac_update_complete(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_update_complete()");
    /* Disable the callback - the update either worked or it didn't,
     * in either case we no longer need to subscribe to MAC packets. */
    wendigo_set_mac_rcvd_callback(NULL);
    WendigoApp* app = (WendigoApp*)context;
    if(app == NULL) {
        wendigo_log(
            MSG_ERROR, "wendigo_scene_setup_mac_update_complete() called with NULL context.");
        return;
    }
    /* Set interface string */
    char result_if_text[IF_MAX_LEN] = "";
    switch(updated_interface) {
    case IF_BT_CLASSIC:
    case IF_BLE:
        strcpy(result_if_text, "Bluetooth");
        break;
    case IF_WIFI:
        strcpy(result_if_text, "WiFi");
        break;
    default:
        strcpy(result_if_text, "UNKNOWN");
        break;
    }
    snprintf(
        popup_header_text,
        strlen("Update  MAC") + strlen(result_if_text) + 1,
        "Update %s MAC",
        result_if_text);
    /* Was the MAC change successful? */
    if(memcmp(app->interfaces[updated_interface].mac_bytes, updated_mac, MAC_BYTES)) {
        /* Received MAC does not match expected MAC */
        snprintf(
            popup_text,
            strlen(result_if_text) + strlen("Failed to Update  MAC!") + 1,
            "Failed to Update %s MAC!",
            result_if_text);
    } else {
        /* Received MAC matches expected MAC */
        snprintf(
            popup_text,
            strlen(result_if_text) + strlen(" MAC Updated!") + 1,
            "%s MAC Updated!",
            result_if_text);
    }
    wendigo_display_popup(app, popup_header_text, popup_text);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_update_complete()");
}

/** Callback invoked when the save button is pressed on the byte input */
void wendigo_scene_setup_mac_input_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_input_callback()");
    /* Finished with the MAC dialogue */
    WendigoApp* app = context;

    /* Did the user change the MAC? */
    if(memcmp(view_bytes, app->interfaces[app->active_interface].mac_bytes, MAC_BYTES)) {
        /* MAC was changed - Figure out which radio is having its MAC changed */
        bool mac_changed;
        ;
        switch(app->active_interface) {
        case IF_BT_CLASSIC:
        case IF_BLE:
        case IF_WIFI:
            mac_changed = true;
            break;
        default:
            mac_changed = false;
            break;
        }
        if(!mac_changed) {
            /* The MAC is different, but I don't know what it's different from */
            FURI_LOG_T(
                WENDIGO_TAG,
                "End wendigo_scene_setup_mac_input_callback() - Invalid MAC changed, going BACK.");
            scene_manager_previous_scene(app->scene_manager);
            return;
        }
        /* Save the interface being changed so I can check it later */
        updated_interface = app->active_interface;
        memcpy(updated_mac, view_bytes, MAC_BYTES);
        wendigo_mac_set(
            app, app->active_interface, updated_mac, wendigo_scene_setup_mac_update_complete);
        /* Wait for completion */
        if(loading != NULL) {
            view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewLoading);
        }
    } else {
        /* MAC not changed */
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendio_scene_setup_mac_input_callback() - MAC unchanged, going BACK.");
        scene_manager_previous_scene(app->scene_manager);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_input_callback()");
}

/** Callback invoked when the contents of the byte input are changed.
 * Here, we check to see whether the displayed MAC is mutable. If it
 * isn't the user's changes are overwritten with a fresh copy of the MAC.
 */
void wendigo_scene_setup_mac_changed_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_changed_callback()");
    WendigoApp* app = context;
    /* Overwrite the change if the MAC isn't mutable */
    if(!app->interfaces[app->active_interface].mutable) {
        memcpy(view_bytes, app->interfaces[app->active_interface].mac_bytes, MAC_BYTES);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_changed_callback()");
}

/** Function called when the scene is being launched. Requests interface MACs if
 * they're uninitialised (the setup scene requests MACs whether or not they're
 * initialised, doing it here feels like it would be too late). If necessary, a
 * loading widget will be displayed until a MAC packet is received.
 */
void wendigo_scene_setup_mac_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_on_enter()");
    WendigoApp* app = context;
    ByteInput* mac_input = app->setup_mac;
    app->current_view = WendigoAppViewSetupMAC;

    /* Initialise the loading view if needed */
    if(loading == NULL) {
        loading = loading_alloc();
        if(loading != NULL) {
            view_dispatcher_add_view(
                app->view_dispatcher, WendigoAppViewLoading, loading_get_view(loading));
        }
    }

    /* If necessary, fetch the interface's MAC first */
    if(!app->interfaces[app->active_interface].initialised) {
        if(loading != NULL) {
            /* Display the loading dialogue */
            view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewLoading);
        }
    }
    /* Pause for 100ms and check whether we've received MACs yet */
    while(!app->interfaces[app->active_interface].initialised) {
        furi_delay_ms(100); // TODO: Review frequency
        wendigo_mac_query(app);
    } /* Hooray - We finished! */

    /* Copy active interface's MAC into a temp array for modification by the view */
    memcpy(view_bytes, app->interfaces[app->active_interface].mac_bytes, MAC_BYTES);

    byte_input_set_header_text(mac_input, "MAC Address");
    byte_input_set_result_callback(
        mac_input,
        wendigo_scene_setup_mac_input_callback,
        wendigo_scene_setup_mac_changed_callback,
        app,
        view_bytes,
        MAC_BYTES);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewSetupMAC);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_on_enter()");
}

/* Declaring this function is part of the macro spaghetti defined in wendigo_scene_config.h
 * so this function needs to exist. */
bool wendigo_scene_setup_mac_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start + End wendigo_scene_setup_mac_on_event()");
    UNUSED(context);
    UNUSED(event);
    return false; /* This function never consumes any events */
}

void wendigo_scene_setup_mac_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_setup_mac_on_exit()");
    WendigoApp* app = (WendigoApp*)context;
    if(loading != NULL) {
        view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewLoading);
        loading_free(loading);
        loading = NULL;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_setup_mac_on_exit()");
}
