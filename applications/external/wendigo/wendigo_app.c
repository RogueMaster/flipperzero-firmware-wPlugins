#include "wendigo_app_i.h"
#include "wendigo_scan.h"

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <math.h>

/* UART rx callback for Console Output scene */
extern void wendigo_console_output_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);

/* Initialiser and terminator for wendigo_scene_device_list.c */
extern void wendigo_scene_device_list_init(void* config);
extern void wendigo_scene_device_list_free();
/* Cleanup function for wendigo_scene_pnl_list.c */
extern void wendigo_scene_pnl_list_free();

/** Ask ESP32-Wendigo to provide MAC details about its interfaces. */
void wendigo_mac_query(WendigoApp* app) {
    wendigo_uart_tx(app->uart, (uint8_t*)"mac\n", 5);
}
/** Function pointer to notify when a MAC packet is received */
void (*mac_rcvd_callback)(void*);

/** Call the MAC received callback */
void wendigo_mac_rcvd_callback(WendigoApp* app) {
    if(mac_rcvd_callback != NULL && app != NULL) {
        mac_rcvd_callback(app);
    }
}

/** Set the callback invoked when a MAC packet is
 * received. NULL to disable.
 */
void wendigo_set_mac_rcvd_callback(void (*update_callback)(void*)) {
    mac_rcvd_callback = update_callback;
}

/** Ask ESP32-Wendigo to change the MAC for the specified interface. */
void wendigo_mac_set(
    WendigoApp* app,
    InterfaceType type,
    uint8_t mac_bytes[MAC_BYTES],
    void (*update_callback)(void*)) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_mac_set()");
    mac_rcvd_callback = update_callback;
    char* cmd = malloc(sizeof(char) * 28);
    char* macStr = malloc(sizeof(char) * (MAC_STRLEN + 1));
    if(cmd == NULL || macStr == NULL) {
        // Unable to allocate %d bytes for MAC and 28 bytes for a command to change MAC. 68
        char* msg = malloc(sizeof(char) * 80);
        if(msg == NULL) {
            wendigo_log(MSG_ERROR, "Unable to allocate memory required to change MAC.");
        } else {
            snprintf(
                msg,
                80,
                "Unable to allocate %d bytes for MAC and %d bytes for command to change MAC.",
                sizeof(char) * (MAC_STRLEN + 1),
                sizeof(char) * 28);
            wendigo_log(MSG_ERROR, msg);
            free(msg);
        }
        if(cmd != NULL) {
            free(cmd);
        }
        if(macStr != NULL) {
            free(macStr);
        }
        return;
    }
    uint8_t ifType;
    switch(type) {
    case IF_BLE:
    case IF_BT_CLASSIC:
        ifType = WENDIGO_MAC_BLUETOOTH;
        break;
    case IF_WIFI:
        ifType = WENDIGO_MAC_WIFI;
        break;
    default:
        ifType = WENDIGO_MAC_BASE;
        break;
    }
    bytes_to_string(mac_bytes, MAC_BYTES, macStr);
    snprintf(cmd, 28, "mac %d %s\n", ifType, macStr);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_mac_set() - Sending command \"%s\"", cmd);
    wendigo_uart_tx(app->uart, (uint8_t*)cmd, strlen(cmd));
    free(macStr);
    free(cmd);
}

static bool wendigo_app_custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_custom_event_callback()");
    furi_assert(context);
    WendigoApp* app = context;
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_custom_event_callback()");
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wendigo_app_back_event_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_back_event_callback()");
    furi_assert(context);
    WendigoApp* app = context;
    /* Set the app->leaving_scene flag on back button press, but only if a
     * Device List scene is currently displayed. */
    if(app->current_view == WendigoAppViewDeviceList ||
       app->current_view == WendigoAppViewPNLDeviceList ||
       app->current_view == WendigoAppViewAPSTAs || app->current_view == WendigoAppViewSTAAP) {
        app->leaving_scene = true;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_back_event_callback()");
    return scene_manager_handle_back_event(app->scene_manager);
}

static void wendigo_app_tick_event_callback(void* context) {
    //    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_tick_event_callback()");
    furi_assert(context);
    WendigoApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
    //    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_tick_event_callback()");
}

/** Return the view, registered with the view dispatcher, that's associated
 * with the specified "logical" view. For example, the main, setup, status,
 * and other menus all use the central WendigoAppViewVarItemList.
 */
WendigoAppView wendigo_appview_for_view(WendigoAppView logicalView) {
    WendigoAppView result;
    switch(logicalView) {
    case WendigoAppViewVarItemList:
    case WendigoAppViewStatus:
    case WendigoAppViewSetup:
    case WendigoAppViewSetupChannel:
    case WendigoAppViewPNLList:
        result = WendigoAppViewVarItemList;
        break;
    default:
        result = logicalView;
        break;
    }
    return result;
}

/** Generic handler for app->popup that restores the previous view */
void wendigo_popup_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_popup_callback()");
    WendigoApp* app = (WendigoApp*)context;
    WendigoAppView view = wendigo_appview_for_view(app->current_view);
    popup_reset(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_popup_callback()");
}

void wendigo_display_popup(WendigoApp* app, char* header, char* body) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_display_popup()");
    // TODO: Review and kill this
    char* newBody = malloc(sizeof(char*) * (strlen(body) + 1));
    strncpy(newBody, body, strlen(body) + 1);
    popup_set_header(app->popup, header, 64, 3, AlignCenter, AlignTop);
    popup_set_text(app->popup, newBody, 64, 22, AlignCenter, AlignTop);
    popup_set_icon(app->popup, -1, -1, NULL); // TODO: Find a fun icon to use
    popup_set_timeout(app->popup, 3000); // was 2000
    popup_enable_timeout(app->popup);
    popup_set_callback(app->popup, wendigo_popup_callback);
    popup_set_context(app->popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewPopup);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_display_popup()");
}

/* Initialise app->interfaces - Default all radios to on */
void wendigo_interface_init(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_interface_init()");
    for(uint8_t i = 0; i < IF_COUNT; ++i) {
        app->interfaces[i].active = true;
        app->interfaces[i].mutable = true;
        app->interfaces[i].scanning = false;
        app->interfaces[i].initialised = false;
    }
    memcpy(app->interfaces[IF_WIFI].mac_bytes, nullMac, MAC_BYTES);
    memcpy(app->interfaces[IF_BT_CLASSIC].mac_bytes, nullMac, MAC_BYTES);
    memcpy(app->interfaces[IF_BLE].mac_bytes, nullMac, MAC_BYTES);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_interface_init()");
}

WendigoApp* wendigo_app_alloc() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_alloc()");
    WendigoApp* app = malloc(sizeof(WendigoApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&wendigo_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, wendigo_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wendigo_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, wendigo_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        WendigoAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    for(uint8_t i = 0; i < START_MENU_ITEMS; ++i) {
        app->selected_option_index[i] = 0;
    }

    for(uint8_t i = 0; i < SETUP_MENU_ITEMS; ++i) {
        app->setup_selected_option_index[i] = 0;
    }

    /* Initialise the channel bitmasks - pow() is slow so hardcode 0 & 1 and use multiplication for the rest */
    app->CH_MASK[0] = 0;
    app->CH_MASK[1] = 1;
    for(uint8_t i = 2; i <= SETUP_CHANNEL_MENU_ITEMS; ++i) {
        app->CH_MASK[i] = app->CH_MASK[i - 1] * 2;
    }

    /* Default to enabling all channels */
    for(uint8_t i = 0; i <= SETUP_CHANNEL_MENU_ITEMS; ++i) {
        /* Bitwise - Add current channel to app->channel_mask */
        app->channel_mask = app->channel_mask | app->CH_MASK[i];
    }
    /* Also set CH_MASK_ALL */
    app->channel_mask = app->channel_mask | CH_MASK_ALL;

    /* Initialise and enable all interfaces */
    wendigo_interface_init(app);
    app->is_scanning = false;

    /* Initialise mutexes */
    app->bufferMutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->devicesMutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->pnlMutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewHelp, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewConsoleOutput, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();
    furi_string_reserve(app->text_box_store, WENDIGO_TEXT_BOX_STORE_SIZE);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewTextInput, text_input_get_view(app->text_input));

    // TODO: Ditch the hex input
    app->hex_input = wendigo_hex_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewHexInput, wendigo_hex_input_get_view(app->hex_input));

    /* Initialise MAC address view */
    app->setup_mac = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewSetupMAC, byte_input_get_view(app->setup_mac));

    /* Initialise the popup */
    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WendigoAppViewPopup, popup_get_view(app->popup));

    /* Device list (all and tagged) */
    app->devices_var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        WendigoAppViewDeviceList,
        variable_item_list_get_view(app->devices_var_item_list));

    /* Initialise the DeviceListInstance struct used in device list */
    wendigo_scene_device_list_init(NULL);
    app->leaving_scene = false;

    /* Initialise the last packet received time */
    app->last_packet = furi_hal_rtc_get_timestamp();

    scene_manager_next_scene(app->scene_manager, WendigoSceneStart);

    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_alloc()");
    return app;
}

void wendigo_app_free(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app_free()");
    furi_assert(app);

    furi_timer_stop(app->scan_timer);
    furi_timer_free(app->scan_timer);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewHelp);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewConsoleOutput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewHexInput);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewSetupMAC);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, WendigoAppViewDeviceList);

    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);
    text_input_free(app->text_input);
    wendigo_hex_input_free(app->hex_input);
    byte_input_free(app->setup_mac);
    popup_free(app->popup);
    variable_item_list_free(app->devices_var_item_list);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    /* Free device list scene's contents and stack */
    wendigo_scene_device_list_free();
    /* Free preferred network list scene's device cache */
    wendigo_scene_pnl_list_free();
    /* Free device cache and UART buffer */
    wendigo_free_uart_buffer();
    wendigo_free_devices();
    /* Mutexes */
    furi_mutex_free(app->bufferMutex);
    furi_mutex_free(app->devicesMutex);
    furi_mutex_free(app->pnlMutex);

    wendigo_uart_free(app->uart);

    // Close records
    furi_record_close(RECORD_GUI);

    free(app);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app_free()");
}

int32_t wendigo_app(void* p) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_app()");
    UNUSED(p);
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    WendigoApp* wendigo_app = wendigo_app_alloc();

    wendigo_app->uart = wendigo_uart_init(wendigo_app);
    /* Set UART callback using wendigo_scan */
    wendigo_uart_set_binary_cb(wendigo_app->uart);

    view_dispatcher_run(wendigo_app->view_dispatcher);

    wendigo_app_free(wendigo_app);

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    FURI_LOG_T(WENDIGO_TAG, "End wendigo_app()");
    return 0;
}

void wendigo_uart_set_binary_cb(Wendigo_Uart* uart) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_set_binary_cb()");
    wendigo_uart_set_handle_rx_data_cb(uart, wendigo_scan_handle_rx_data_cb);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_set_binary_cb()");
}

void wendigo_uart_set_console_cb(Wendigo_Uart* uart) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_uart_set_console_cb()");
    wendigo_uart_set_handle_rx_data_cb(uart, wendigo_console_output_handle_rx_data_cb);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_uart_set_console_cb()");
}

/* Convert an array of bytesCount uint8_ts into a colon-separated string of bytes.
   strBytes must be initialised with sufficient space to hold the output string.
   For a MAC this is 18 bytes. In general it is 3 * byteCount */
void bytes_to_string(uint8_t* bytes, uint16_t bytesCount, char* strBytes) {
    FURI_LOG_T(WENDIGO_TAG, "Start bytes_to_string()");
    uint8_t* p_in = bytes;
    const char* hex = "0123456789ABCDEF";
    char* p_out = strBytes;
    for(; p_in < bytes + bytesCount; p_out += 3, ++p_in) {
        p_out[0] = hex[(*p_in >> 4) & 0xF];
        p_out[1] = hex[*p_in & 0xF];
        p_out[2] = ':';
    }
    p_out[-1] = 0;
    FURI_LOG_T(WENDIGO_TAG, "End bytes_to_string()");
}

/** Return a human-readable representation of a FuriStatus value. The provided
 * string `result` should be an initialised string with capacity `resultLen`.
 * result should have a sufficient length to cater for the statuses that are
 * expected to be received, although the result will be truncated to stay within
 * the specified length.
 * The maximum string length returned by this function (excluding the terminating
 * NULL byte) is 28, however for practical purposes - unless you're developing new
 * functions that use this function - can be considered to be 22.
 * The resulting string is copied into result, and a pointer to result is provided
 * as the return value.
 */
char* furi_status_to_string(FuriStatus status, char* result, uint8_t resultLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start furi_status_to_string()");
    if(result == NULL || strlen(result) == 0 || resultLen == 0) {
        wendigo_log(MSG_ERROR, "Invalid arguments provided to furi_status_to_string()");
        FURI_LOG_T(WENDIGO_TAG, "End furi_status_to_string()");
        return NULL;
    }
    bzero(result, resultLen);
    switch(status) {
    case FuriStatusOk:
        strncpy(result, "OK", resultLen);
        break;
    case FuriStatusError:
        strncpy(result, "Error", resultLen);
        break;
    case FuriStatusErrorTimeout:
        strncpy(result, "Error: Timeout", resultLen);
        break;
    case FuriStatusErrorResource:
        strncpy(result, "Resource Not Available", resultLen);
        break;
    case FuriStatusErrorParameter:
        strncpy(result, "Parameter Error", resultLen);
        break;
    case FuriStatusErrorNoMemory:
        strncpy(result, "Out of Memory", resultLen);
        break;
    case FuriStatusErrorISR:
        strncpy(result, "Cannot be called from an ISR", resultLen);
        break;
    default:
        strncpy(result, "Unknown Error", resultLen);
        break;
    }
    FURI_LOG_T(WENDIGO_TAG, "End furi_status_to_string()");
    return result;
}
