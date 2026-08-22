#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../toshiba_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ToshibaMainView ToshibaMainView;

// Debounce delay for temperature changes (ms)
#define TOSHIBA_SEND_DEBOUNCE_MS 800

typedef void (*ToshibaMainViewSendCallback)(void* context);
typedef void (*ToshibaMainViewNavigateCallback)(void* context);

ToshibaMainView* toshiba_main_view_alloc(void);
void toshiba_main_view_free(ToshibaMainView* view);
View* toshiba_main_view_get_view(ToshibaMainView* view);
void toshiba_main_view_set_state(ToshibaMainView* view, ToshibaState* state);

void toshiba_main_view_set_send_callback(
    ToshibaMainView* view,
    ToshibaMainViewSendCallback callback,
    void* context);
void toshiba_main_view_set_extra_callback(
    ToshibaMainView* view,
    ToshibaMainViewNavigateCallback callback,
    void* context);
void toshiba_main_view_set_setup_callback(
    ToshibaMainView* view,
    ToshibaMainViewNavigateCallback callback,
    void* context);

void toshiba_main_view_start_sending(ToshibaMainView* view);
void toshiba_main_view_update_sending(ToshibaMainView* view);
void toshiba_main_view_stop_sending(ToshibaMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int toshiba_main_view_get_last_command(ToshibaMainView* view, ToshibaToggle* out_toggle);

#ifdef __cplusplus
}
#endif
