#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../daikin_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DaikinMainView DaikinMainView;

// Debounce delay for temperature changes (ms)
#define DAIKIN_SEND_DEBOUNCE_MS 800

typedef void (*DaikinMainViewSendCallback)(void* context);
typedef void (*DaikinMainViewNavigateCallback)(void* context);

DaikinMainView* daikin_main_view_alloc(void);
void daikin_main_view_free(DaikinMainView* view);
View* daikin_main_view_get_view(DaikinMainView* view);
void daikin_main_view_set_state(DaikinMainView* view, DaikinState* state);

void daikin_main_view_set_send_callback(
    DaikinMainView* view,
    DaikinMainViewSendCallback callback,
    void* context);
void daikin_main_view_set_extra_callback(
    DaikinMainView* view,
    DaikinMainViewNavigateCallback callback,
    void* context);
void daikin_main_view_set_setup_callback(
    DaikinMainView* view,
    DaikinMainViewNavigateCallback callback,
    void* context);

void daikin_main_view_start_sending(DaikinMainView* view);
void daikin_main_view_update_sending(DaikinMainView* view);
void daikin_main_view_stop_sending(DaikinMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int daikin_main_view_get_last_command(DaikinMainView* view, DaikinToggle* out_toggle);

#ifdef __cplusplus
}
#endif
