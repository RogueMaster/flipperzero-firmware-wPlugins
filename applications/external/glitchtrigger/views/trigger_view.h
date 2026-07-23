#pragma once

#include <gui/view.h>
#include "../helpers/glitch_config.h"

/* The main fire screen: a schematic pulse timeline (trigger -> delay -> the
 * glitch pulse train), an ARM/FIRE state badge, and a live shot counter. */

typedef struct TriggerView TriggerView;

typedef enum {
    TriggerStateIdle, // disarmed
    TriggerStateArmed, // manual: output live, OK fires a shot
    TriggerStateWaiting, // external: interrupt armed, waiting for an edge
    TriggerStateRunning, // repeat: free-running
} TriggerState;

typedef enum {
    TriggerViewEventOk,
    TriggerViewEventLeft,
    TriggerViewEventRight,
} TriggerViewEvent;

typedef void (*TriggerViewCallback)(TriggerViewEvent event, void* ctx);

TriggerView* trigger_view_alloc(void);
void trigger_view_free(TriggerView* v);
View* trigger_view_get_view(TriggerView* v);

void trigger_view_set_callback(TriggerView* v, TriggerViewCallback cb, void* ctx);
void trigger_view_set_params(TriggerView* v, const GlitchParams* p);
void trigger_view_set_state(TriggerView* v, TriggerState state);
void trigger_view_set_shots(TriggerView* v, uint32_t shots);
void trigger_view_flash(TriggerView* v); // pop a brief "fired" highlight
void trigger_view_tick(TriggerView* v); // advance blink / decay flash
