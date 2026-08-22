#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../ballu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BalluMainView BalluMainView;

// Debounce delay for temperature changes (ms)
#define BALLU_SEND_DEBOUNCE_MS 800

typedef void (*BalluMainViewSendCallback)(void* context);
typedef void (*BalluMainViewNavigateCallback)(void* context);

BalluMainView* ballu_main_view_alloc(void);
void ballu_main_view_free(BalluMainView* view);
View* ballu_main_view_get_view(BalluMainView* view);
void ballu_main_view_set_state(BalluMainView* view, BalluState* state);

void ballu_main_view_set_send_callback(
    BalluMainView* view,
    BalluMainViewSendCallback callback,
    void* context);
void ballu_main_view_set_extra_callback(
    BalluMainView* view,
    BalluMainViewNavigateCallback callback,
    void* context);
void ballu_main_view_set_setup_callback(
    BalluMainView* view,
    BalluMainViewNavigateCallback callback,
    void* context);

void ballu_main_view_start_sending(BalluMainView* view);
void ballu_main_view_update_sending(BalluMainView* view);
void ballu_main_view_stop_sending(BalluMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int ballu_main_view_get_last_command(BalluMainView* view, BalluToggle* out_toggle);

#ifdef __cplusplus
}
#endif
