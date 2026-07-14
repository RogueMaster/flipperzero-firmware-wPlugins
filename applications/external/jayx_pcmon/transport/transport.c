#include "../jayx.h"

void jayx_usb_start(JayxApp* app);
void jayx_usb_stop(JayxApp* app);
void jayx_bt_start(JayxApp* app);
void jayx_bt_stop(JayxApp* app);

void jayx_transport_start(JayxApp* app, JayxTransport t) {
    furi_assert(app);
    if(app->transport != JayxTransportNone) {
        jayx_transport_stop(app);
    }

    app->rx_len = 0;
    app->rx_pending = false;
    if(app->rx_stream) {
        furi_stream_buffer_reset(app->rx_stream);
    }
    memset(&app->data, 0, sizeof(app->data));

    app->transport = t;
    if(t == JayxTransportUsb) {
        jayx_usb_start(app);
    } else if(t == JayxTransportBt) {
        jayx_bt_start(app);
    }

    app->ui_state = JayxUiWaiting;
    app->page = JayxPageSystem;
}

void jayx_transport_stop(JayxApp* app) {
    furi_assert(app);

    if(app->transport == JayxTransportUsb) {
        jayx_usb_stop(app);
    } else if(app->transport == JayxTransportBt) {
        jayx_bt_stop(app);
    }

    app->transport = JayxTransportNone;
    app->rx_len = 0;
    app->rx_pending = false;
}
