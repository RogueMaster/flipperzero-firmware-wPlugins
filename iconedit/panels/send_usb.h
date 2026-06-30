#pragma once

#include <strings.h> // for size_t
#include "../icon.h"
#include "../iconedit.h"

typedef enum {
    SendAsC,
    SendAsPNG,
    SendAsBMX,
} SendAsType;

void send_usb_start(IEIcon* icon, SendAsType send_as, bool current_frame_only);
void send_usb_set_update_callback(IconEditUpdateCallback callback, void* context);
