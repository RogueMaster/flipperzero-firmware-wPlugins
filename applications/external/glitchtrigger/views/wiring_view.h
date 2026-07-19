#pragma once

#include <gui/view.h>
#include "../helpers/glitch_config.h"

/* A labelled hook-up diagram: Flipper GPIO -> gate driver / MOSFET crowbar ->
 * target rail, with a common ground, plus a rotating one-line safety tip. */

typedef struct WiringView WiringView;

WiringView* wiring_view_alloc(void);
void wiring_view_free(WiringView* v);
View* wiring_view_get_view(WiringView* v);

void wiring_view_set_pins(
    WiringView* v,
    uint8_t out_header,
    const char* out_name,
    uint8_t in_header,
    const char* in_name,
    bool show_trigger_in);
void wiring_view_tick(WiringView* v); // rotate the safety tip
