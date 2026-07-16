#pragma once

#include <gui/view.h>

typedef struct WiringView WiringView;

WiringView* wiring_view_alloc(void);
void wiring_view_free(WiringView* wv);
View* wiring_view_get_view(WiringView* wv);

/** Which pins the current port profile actually uses, e.g. 14 and 13. */
void wiring_view_set_pins(WiringView* wv, uint8_t rx_pin, uint8_t tx_pin);
