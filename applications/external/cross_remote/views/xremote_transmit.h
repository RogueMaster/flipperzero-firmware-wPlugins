#pragma once

#include "../xremote.h"
#include "../helpers/xremote_custom_event.h"

typedef struct XRemoteTransmit XRemoteTransmit;

typedef void (*XRemoteTransmitCallback)(XRemoteCustomEvent event, void* context);

void xremote_transmit_model_set_name(XRemoteTransmit* instance, const char* name);
void xremote_transmit_model_set_type(XRemoteTransmit* instance, int type);
// Seconds remaining for the active pause; shown as an MM:SS countdown.
void xremote_transmit_model_set_remaining(XRemoteTransmit* instance, int remaining);

void xremote_transmit_set_callback(
    XRemoteTransmit* instance,
    XRemoteTransmitCallback callback,
    void* context);

XRemoteTransmit* xremote_transmit_alloc();

void xremote_transmit_free(XRemoteTransmit* instance);

View* xremote_transmit_get_view(XRemoteTransmit* instance);

void xremote_transmit_enter(void* context);

bool xremote_transmit_input(InputEvent* event, void* context);
