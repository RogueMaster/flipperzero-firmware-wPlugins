/**
 * RollCall - band hunt screen.
 *
 * "I pressed my remote and nothing happened" is nearly always the wrong
 * frequency. This screen sweeps every candidate band while you hold the button
 * down and draws, per band, how far its loudest reading climbed above its own
 * noise floor. The band your fob actually transmits on towers over the rest.
 *
 * OK adopts the winner and runs the health check there. It only ever reports a
 * band that genuinely beat its floor - no signal means no answer, not a guess.
 */
#pragma once

#include <gui/view.h>
#include "../helpers/rc_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HuntEventAdopt, // user accepted the winning band
} HuntEvent;

typedef void (*HuntViewCallback)(void* context, HuntEvent event);

typedef struct HuntView HuntView;

HuntView* hunt_view_alloc(void);
void hunt_view_free(HuntView* v);
View* hunt_view_get_view(HuntView* v);

void hunt_view_set_callback(HuntView* v, HuntViewCallback cb, void* context);

/** Push a fresh sweep snapshot. `best` is an rc_bands index, or -1 for none. */
void hunt_view_set_data(
    HuntView* v,
    const RcHuntBand* bands,
    uint8_t count,
    int8_t best,
    uint32_t sweeps);

void hunt_view_reset(HuntView* v);
void hunt_view_tick(HuntView* v);

#ifdef __cplusplus
}
#endif
