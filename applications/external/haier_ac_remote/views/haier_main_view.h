#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../haier_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HaierMainView HaierMainView;

// Debounce delay for temperature changes (ms)
#define HAIER_SEND_DEBOUNCE_MS 800

typedef void (*HaierMainViewSendCallback)(void* context);
typedef void (*HaierMainViewNavigateCallback)(void* context);

HaierMainView* haier_main_view_alloc(void);
void haier_main_view_free(HaierMainView* view);
View* haier_main_view_get_view(HaierMainView* view);
void haier_main_view_set_state(HaierMainView* view, HaierState* state);

void haier_main_view_set_send_callback(
    HaierMainView* view,
    HaierMainViewSendCallback callback,
    void* context);
void haier_main_view_set_extra_callback(
    HaierMainView* view,
    HaierMainViewNavigateCallback callback,
    void* context);
void haier_main_view_set_setup_callback(
    HaierMainView* view,
    HaierMainViewNavigateCallback callback,
    void* context);

void haier_main_view_start_sending(HaierMainView* view);
void haier_main_view_update_sending(HaierMainView* view);
void haier_main_view_stop_sending(HaierMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int haier_main_view_get_last_command(HaierMainView* view, HaierToggle* out_toggle);

#ifdef __cplusplus
}
#endif
