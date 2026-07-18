#pragma once

#include <gui/view.h>
#include "../helpers/ir_sense.h"

typedef struct ProbeView ProbeView;

ProbeView* probe_view_alloc(void);
void probe_view_free(ProbeView* v);
View* probe_view_get_view(ProbeView* v);

/* pin_name/pin_number label the wiring page; detected/mv drive the live check. */
void probe_view_update(
    ProbeView* v,
    const char* pin_name,
    uint8_t pin_number,
    bool detected,
    uint16_t mv);
void probe_view_tick(ProbeView* v);
