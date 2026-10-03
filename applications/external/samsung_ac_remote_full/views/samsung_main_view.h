#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../samsung_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SamsungMainView SamsungMainView;

// Debounce delay for temperature changes (ms)
#define SAMSUNG_SEND_DEBOUNCE_MS 800

typedef void (*SamsungMainViewSendCallback)(void* context);
typedef void (*SamsungMainViewNavigateCallback)(void* context);

SamsungMainView* samsung_main_view_alloc(void);
void samsung_main_view_free(SamsungMainView* view);
View* samsung_main_view_get_view(SamsungMainView* view);
void samsung_main_view_set_state(SamsungMainView* view, SamsungState* state);

void samsung_main_view_set_send_callback(
    SamsungMainView* view,
    SamsungMainViewSendCallback callback,
    void* context);
void samsung_main_view_set_extra_callback(
    SamsungMainView* view,
    SamsungMainViewNavigateCallback callback,
    void* context);
void samsung_main_view_set_setup_callback(
    SamsungMainView* view,
    SamsungMainViewNavigateCallback callback,
    void* context);

void samsung_main_view_start_sending(SamsungMainView* view);
void samsung_main_view_update_sending(SamsungMainView* view);
void samsung_main_view_stop_sending(SamsungMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int samsung_main_view_get_last_command(SamsungMainView* view, SamsungToggle* out_toggle);

#ifdef __cplusplus
}
#endif
