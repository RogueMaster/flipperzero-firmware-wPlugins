#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../kelvinator_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KelvinatorMainView KelvinatorMainView;

// Debounce delay for temperature changes (ms)
#define KELVINATOR_SEND_DEBOUNCE_MS 800

typedef void (*KelvinatorMainViewSendCallback)(void* context);
typedef void (*KelvinatorMainViewNavigateCallback)(void* context);

KelvinatorMainView* kelvinator_main_view_alloc(void);
void kelvinator_main_view_free(KelvinatorMainView* view);
View* kelvinator_main_view_get_view(KelvinatorMainView* view);
void kelvinator_main_view_set_state(KelvinatorMainView* view, KelvinatorState* state);

void kelvinator_main_view_set_send_callback(
    KelvinatorMainView* view,
    KelvinatorMainViewSendCallback callback,
    void* context);
void kelvinator_main_view_set_extra_callback(
    KelvinatorMainView* view,
    KelvinatorMainViewNavigateCallback callback,
    void* context);
void kelvinator_main_view_set_setup_callback(
    KelvinatorMainView* view,
    KelvinatorMainViewNavigateCallback callback,
    void* context);

void kelvinator_main_view_start_sending(KelvinatorMainView* view);
void kelvinator_main_view_update_sending(KelvinatorMainView* view);
void kelvinator_main_view_stop_sending(KelvinatorMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int kelvinator_main_view_get_last_command(KelvinatorMainView* view, KelvinatorToggle* out_toggle);

#ifdef __cplusplus
}
#endif
