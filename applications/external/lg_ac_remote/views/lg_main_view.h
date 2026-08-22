#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../lg_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LgMainView LgMainView;

// Debounce delay for temperature changes (ms)
#define LG_SEND_DEBOUNCE_MS 800

typedef void (*LgMainViewSendCallback)(void* context);
typedef void (*LgMainViewNavigateCallback)(void* context);

LgMainView* lg_main_view_alloc(void);
void lg_main_view_free(LgMainView* view);
View* lg_main_view_get_view(LgMainView* view);
void lg_main_view_set_state(LgMainView* view, LgState* state);

void lg_main_view_set_send_callback(
    LgMainView* view,
    LgMainViewSendCallback callback,
    void* context);
void lg_main_view_set_extra_callback(
    LgMainView* view,
    LgMainViewNavigateCallback callback,
    void* context);
void lg_main_view_set_setup_callback(
    LgMainView* view,
    LgMainViewNavigateCallback callback,
    void* context);

void lg_main_view_start_sending(LgMainView* view);
void lg_main_view_update_sending(LgMainView* view);
void lg_main_view_stop_sending(LgMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int lg_main_view_get_last_command(LgMainView* view, LgToggle* out_toggle);

#ifdef __cplusplus
}
#endif
