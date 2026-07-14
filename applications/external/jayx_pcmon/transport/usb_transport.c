#include "../jayx.h"

#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>

static void usb_cdc_rx_callback(void* context) {
    furi_assert(context);
    JayxApp* app = context;

    uint8_t buf[64];
    int32_t n = furi_hal_cdc_receive(0, buf, sizeof(buf));
    while(n > 0) {
        jayx_feed_bytes(app, buf, (size_t)n);
        n = furi_hal_cdc_receive(0, buf, sizeof(buf));
    }
    app->rx_pending = true;
}

void jayx_usb_start(JayxApp* app) {
    app->previous_usb_config = furi_hal_usb_get_config();
    if(!furi_hal_usb_set_config(&usb_cdc_single, NULL)) {
        FURI_LOG_E(TAG, "USB CDC config failed");
    }

    static CdcCallbacks cdc_cb = {
        .tx_ep_callback = NULL,
        .rx_ep_callback = usb_cdc_rx_callback,
        .state_callback = NULL,
        .ctrl_line_callback = NULL,
        .config_callback = NULL,
    };
    furi_hal_cdc_set_callbacks(0, &cdc_cb, app);
}

void jayx_usb_stop(JayxApp* app) {
    furi_hal_cdc_set_callbacks(0, NULL, NULL);
    if(app->previous_usb_config) {
        if(!furi_hal_usb_set_config(app->previous_usb_config, NULL)) {
            FURI_LOG_E(TAG, "USB CDC restore failed");
        }
        app->previous_usb_config = NULL;
    }
}
