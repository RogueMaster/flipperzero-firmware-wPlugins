#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../gree_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GreeMainView GreeMainView;

// Debounce delay for temperature changes (ms)
#define GREE_SEND_DEBOUNCE_MS 800

typedef void (*GreeMainViewSendCallback)(void* context);
typedef void (*GreeMainViewNavigateCallback)(void* context);

GreeMainView* gree_main_view_alloc(void);
void gree_main_view_free(GreeMainView* view);
View* gree_main_view_get_view(GreeMainView* view);
void gree_main_view_set_state(GreeMainView* view, GreeState* state);

void gree_main_view_set_send_callback(
    GreeMainView* view,
    GreeMainViewSendCallback callback,
    void* context);
void gree_main_view_set_extra_callback(
    GreeMainView* view,
    GreeMainViewNavigateCallback callback,
    void* context);
void gree_main_view_set_setup_callback(
    GreeMainView* view,
    GreeMainViewNavigateCallback callback,
    void* context);

void gree_main_view_start_sending(GreeMainView* view);
void gree_main_view_update_sending(GreeMainView* view);
void gree_main_view_stop_sending(GreeMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int gree_main_view_get_last_command(GreeMainView* view, GreeToggle* out_toggle);

#ifdef __cplusplus
}
#endif
