#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../kelon_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KelonMainView KelonMainView;

// Debounce delay for temperature changes (ms)
#define KELON_SEND_DEBOUNCE_MS 800

typedef void (*KelonMainViewSendCallback)(void* context);
typedef void (*KelonMainViewNavigateCallback)(void* context);

KelonMainView* kelon_main_view_alloc(void);
void kelon_main_view_free(KelonMainView* view);
View* kelon_main_view_get_view(KelonMainView* view);
void kelon_main_view_set_state(KelonMainView* view, KelonState* state);

void kelon_main_view_set_send_callback(
    KelonMainView* view,
    KelonMainViewSendCallback callback,
    void* context);
void kelon_main_view_set_extra_callback(
    KelonMainView* view,
    KelonMainViewNavigateCallback callback,
    void* context);
void kelon_main_view_set_setup_callback(
    KelonMainView* view,
    KelonMainViewNavigateCallback callback,
    void* context);

void kelon_main_view_start_sending(KelonMainView* view);
void kelon_main_view_update_sending(KelonMainView* view);
void kelon_main_view_stop_sending(KelonMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int kelon_main_view_get_last_command(KelonMainView* view, KelonToggle* out_toggle);

#ifdef __cplusplus
}
#endif
