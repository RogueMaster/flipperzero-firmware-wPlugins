#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../neoclima_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NeoclimaMainView NeoclimaMainView;

// Debounce delay for temperature changes (ms)
#define NEOCLIMA_SEND_DEBOUNCE_MS 800

typedef void (*NeoclimaMainViewSendCallback)(void* context);
typedef void (*NeoclimaMainViewNavigateCallback)(void* context);

NeoclimaMainView* neoclima_main_view_alloc(void);
void neoclima_main_view_free(NeoclimaMainView* view);
View* neoclima_main_view_get_view(NeoclimaMainView* view);
void neoclima_main_view_set_state(NeoclimaMainView* view, NeoclimaState* state);

void neoclima_main_view_set_send_callback(
    NeoclimaMainView* view,
    NeoclimaMainViewSendCallback callback,
    void* context);
void neoclima_main_view_set_extra_callback(
    NeoclimaMainView* view,
    NeoclimaMainViewNavigateCallback callback,
    void* context);
void neoclima_main_view_set_setup_callback(
    NeoclimaMainView* view,
    NeoclimaMainViewNavigateCallback callback,
    void* context);

void neoclima_main_view_start_sending(NeoclimaMainView* view);
void neoclima_main_view_update_sending(NeoclimaMainView* view);
void neoclima_main_view_stop_sending(NeoclimaMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int neoclima_main_view_get_last_command(NeoclimaMainView* view, NeoclimaToggle* out_toggle);

#ifdef __cplusplus
}
#endif
