#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../fujitsu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FujitsuMainView FujitsuMainView;

// Debounce delay for temperature changes (ms)
#define FUJITSU_SEND_DEBOUNCE_MS 800

typedef void (*FujitsuMainViewSendCallback)(void* context);
typedef void (*FujitsuMainViewNavigateCallback)(void* context);

FujitsuMainView* fujitsu_main_view_alloc(void);
void fujitsu_main_view_free(FujitsuMainView* view);
View* fujitsu_main_view_get_view(FujitsuMainView* view);
void fujitsu_main_view_set_state(FujitsuMainView* view, FujitsuState* state);

void fujitsu_main_view_set_send_callback(
    FujitsuMainView* view,
    FujitsuMainViewSendCallback callback,
    void* context);
void fujitsu_main_view_set_extra_callback(
    FujitsuMainView* view,
    FujitsuMainViewNavigateCallback callback,
    void* context);
void fujitsu_main_view_set_setup_callback(
    FujitsuMainView* view,
    FujitsuMainViewNavigateCallback callback,
    void* context);

void fujitsu_main_view_start_sending(FujitsuMainView* view);
void fujitsu_main_view_update_sending(FujitsuMainView* view);
void fujitsu_main_view_stop_sending(FujitsuMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int fujitsu_main_view_get_last_command(FujitsuMainView* view, FujitsuToggle* out_toggle);

#ifdef __cplusplus
}
#endif
