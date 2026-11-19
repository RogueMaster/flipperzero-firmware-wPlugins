#include "updi_app_i.h"

#include "updi/updi_constants.h"
#include "updi/updi_transport.h"

#include <cli/cli_vcp.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>

#include <stdio.h>
#include <string.h>

#define UPDI_VCP_CDC_CH  1
#define UPDI_VCP_PKT_LEN CDC_DATA_SZ

typedef enum {
    UpdiBridgeEvtStop = (1 << 0),
    UpdiBridgeEvtRx = (1 << 1),
    UpdiBridgeEvtTx = (1 << 2),
    UpdiBridgeEvtState = (1 << 3),
} UpdiBridgeEvt;

typedef struct {
    UpdiApp* app;
    UpdiTransport* transport;
    FuriThread* thread;
    CliVcp* cli_vcp;
    volatile bool running;
    volatile bool usb_connected;
} UpdiBridge;

static void updi_bridge_on_cdc_tx_complete(void* context);
static void updi_bridge_on_cdc_rx(void* context);
static void updi_bridge_state_callback(void* context, uint8_t state);
static void updi_bridge_on_cdc_control_line(void* context, uint8_t state);
static void updi_bridge_on_line_config(void* context, struct usb_cdc_line_coding* config);

static const CdcCallbacks updi_cdc_cb = {
    updi_bridge_on_cdc_tx_complete,
    updi_bridge_on_cdc_rx,
    updi_bridge_state_callback,
    updi_bridge_on_cdc_control_line,
    updi_bridge_on_line_config,
};

static void updi_bridge_on_cdc_tx_complete(void* context) {
    UpdiBridge* bridge = context;
    furi_thread_flags_set(furi_thread_get_id(bridge->thread), UpdiBridgeEvtTx);
}

static void updi_bridge_on_cdc_rx(void* context) {
    UpdiBridge* bridge = context;
    furi_thread_flags_set(furi_thread_get_id(bridge->thread), UpdiBridgeEvtRx);
}

static void updi_bridge_state_callback(void* context, uint8_t state) {
    UpdiBridge* bridge = context;
    bridge->usb_connected = state != 0;
    furi_thread_flags_set(furi_thread_get_id(bridge->thread), UpdiBridgeEvtState);
}

static void updi_bridge_on_cdc_control_line(void* context, uint8_t state) {
    UNUSED(context);
    UNUSED(state);
}

static void updi_bridge_on_line_config(void* context, struct usb_cdc_line_coding* config) {
    UpdiBridge* bridge = context;
    if(bridge->transport == NULL || !updi_transport_is_open(bridge->transport)) return;
    uint32_t baud = config->dwDTERate;
    if(baud < 9600) baud = UPDI_BAUD_SAFE;
    updi_transport_set_baud(bridge->transport, baud);
}

static void updi_bridge_vcp_init(UpdiBridge* bridge) {
    bridge->cli_vcp = furi_record_open(RECORD_CLI_VCP);
    cli_vcp_disable(bridge->cli_vcp);
    furi_check(furi_hal_usb_set_config(&usb_cdc_dual, NULL) == true);
    furi_hal_usb_lock();
    cli_vcp_enable(bridge->cli_vcp);
    furi_hal_cdc_set_callbacks(UPDI_VCP_CDC_CH, (CdcCallbacks*)&updi_cdc_cb, bridge);
}

static void updi_bridge_vcp_deinit(UpdiBridge* bridge) {
    furi_hal_cdc_set_callbacks(UPDI_VCP_CDC_CH, NULL, NULL);
    cli_vcp_disable(bridge->cli_vcp);
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_cdc_single, NULL) == true);
    cli_vcp_enable(bridge->cli_vcp);
    furi_record_close(RECORD_CLI_VCP);
    bridge->cli_vcp = NULL;
}

static void updi_bridge_refresh_widget(UpdiBridge* bridge) {
    UpdiApp* app = bridge->app;
    widget_reset(app->widget);
    char text[192];
    snprintf(
        text,
        sizeof(text),
        "SerialUPDI bridge\n"
        "Pins %s @ %lu\n"
        "USB %s\n"
        "avrdude -c serialupdi\n"
        "Back to stop",
        updi_pins_label(app->pins),
        (unsigned long)(bridge->transport ? updi_transport_baud(bridge->transport) :
                                            UPDI_BAUD_SAFE),
        bridge->usb_connected ? "connected" : "waiting");
    widget_add_string_multiline_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, text);
}

static int32_t updi_bridge_thread(void* context) {
    UpdiBridge* bridge = context;
    UpdiApp* app = bridge->app;

    bridge->transport = updi_transport_alloc();
    if(bridge->transport == NULL) {
        snprintf(app->result, sizeof(app->result), "Out of memory");
        return 0;
    }
    updi_transport_set_pins(bridge->transport, app->pins);
    if(!updi_transport_open(bridge->transport)) {
        snprintf(app->result, sizeof(app->result), "%s", updi_transport_error(bridge->transport));
        updi_transport_free(bridge->transport);
        bridge->transport = NULL;
        return 0;
    }

    updi_bridge_vcp_init(bridge);
    updi_transport_double_break(bridge->transport);
    updi_bridge_refresh_widget(bridge);

    uint8_t buf[UPDI_VCP_PKT_LEN];
    while(bridge->running) {
        uint32_t events = furi_thread_flags_wait(
            UpdiBridgeEvtStop | UpdiBridgeEvtRx | UpdiBridgeEvtTx | UpdiBridgeEvtState,
            FuriFlagWaitAny,
            50);

        if(events & UpdiBridgeEvtStop) break;

        if(events & UpdiBridgeEvtState) {
            app->usb_connected = bridge->usb_connected;
            updi_bridge_refresh_widget(bridge);
        }

        if(events & UpdiBridgeEvtRx) {
            size_t len = furi_hal_cdc_receive(UPDI_VCP_CDC_CH, buf, UPDI_VCP_PKT_LEN);
            if(len > 0) {
                updi_transport_send_raw(bridge->transport, buf, len);
            }
        }

        size_t nread = updi_transport_recv_available(bridge->transport, buf, sizeof(buf), 0);
        if(nread > 0) {
            furi_hal_cdc_send(UPDI_VCP_CDC_CH, buf, (uint16_t)nread);
        }
    }

    updi_bridge_vcp_deinit(bridge);
    updi_transport_close(bridge->transport);
    updi_transport_free(bridge->transport);
    bridge->transport = NULL;
    return 0;
}

static void updi_bridge_stop(UpdiApp* app) {
    UpdiBridge* bridge = app->usb_bridge;
    if(bridge != NULL) {
        bridge->running = false;
        if(bridge->thread != NULL) {
            furi_thread_flags_set(furi_thread_get_id(bridge->thread), UpdiBridgeEvtStop);
            furi_thread_join(bridge->thread);
            furi_thread_free(bridge->thread);
            bridge->thread = NULL;
        }
        free(bridge);
        app->usb_bridge = NULL;
    }
    app->thread = NULL;
}

void updi_usb_bridge_scene_on_enter(void* context) {
    UpdiApp* app = context;
    app->usb_connected = false;
    app->result[0] = '\0';

    UpdiBridge* bridge = malloc(sizeof(UpdiBridge));
    furi_check(bridge);
    memset(bridge, 0, sizeof(UpdiBridge));
    bridge->app = app;
    bridge->running = true;
    bridge->thread = furi_thread_alloc_ex("UpdiUsbBridge", 2 * 1024, updi_bridge_thread, bridge);
    app->usb_bridge = bridge;
    app->thread = bridge->thread;

    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, "Starting SerialUPDI\nbridge...");
    view_dispatcher_switch_to_view(app->view_dispatcher, UpdiViewWidget);
    furi_thread_start(bridge->thread);
}

bool updi_usb_bridge_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void updi_usb_bridge_scene_on_exit(void* context) {
    UpdiApp* app = context;
    updi_bridge_stop(app);
    widget_reset(app->widget);
}
