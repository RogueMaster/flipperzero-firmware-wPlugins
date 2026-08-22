#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../midea_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MideaMainView MideaMainView;

// Debounce delay for temperature changes (ms)
#define MIDEA_SEND_DEBOUNCE_MS 800

typedef void (*MideaMainViewSendCallback)(void* context);
typedef void (*MideaMainViewNavigateCallback)(void* context);

MideaMainView* midea_main_view_alloc(void);
void midea_main_view_free(MideaMainView* view);
View* midea_main_view_get_view(MideaMainView* view);
void midea_main_view_set_state(MideaMainView* view, MideaState* state);

void midea_main_view_set_send_callback(
    MideaMainView* view,
    MideaMainViewSendCallback callback,
    void* context);
void midea_main_view_set_extra_callback(
    MideaMainView* view,
    MideaMainViewNavigateCallback callback,
    void* context);
void midea_main_view_set_setup_callback(
    MideaMainView* view,
    MideaMainViewNavigateCallback callback,
    void* context);

void midea_main_view_start_sending(MideaMainView* view);
void midea_main_view_update_sending(MideaMainView* view);
void midea_main_view_stop_sending(MideaMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int midea_main_view_get_last_command(MideaMainView* view, MideaToggle* out_toggle);

#ifdef __cplusplus
}
#endif
