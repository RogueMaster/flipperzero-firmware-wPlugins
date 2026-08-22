#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../tcl_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TclMainView TclMainView;

// Debounce delay for temperature changes (ms)
#define TCL_SEND_DEBOUNCE_MS 800

typedef void (*TclMainViewSendCallback)(void* context);
typedef void (*TclMainViewNavigateCallback)(void* context);

TclMainView* tcl_main_view_alloc(void);
void tcl_main_view_free(TclMainView* view);
View* tcl_main_view_get_view(TclMainView* view);
void tcl_main_view_set_state(TclMainView* view, TclState* state);

void tcl_main_view_set_send_callback(
    TclMainView* view,
    TclMainViewSendCallback callback,
    void* context);
void tcl_main_view_set_extra_callback(
    TclMainView* view,
    TclMainViewNavigateCallback callback,
    void* context);
void tcl_main_view_set_setup_callback(
    TclMainView* view,
    TclMainViewNavigateCallback callback,
    void* context);

void tcl_main_view_start_sending(TclMainView* view);
void tcl_main_view_update_sending(TclMainView* view);
void tcl_main_view_stop_sending(TclMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int tcl_main_view_get_last_command(TclMainView* view, TclToggle* out_toggle);

#ifdef __cplusplus
}
#endif
