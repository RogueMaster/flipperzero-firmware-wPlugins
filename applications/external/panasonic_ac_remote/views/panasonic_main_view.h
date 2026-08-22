#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../panasonic_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PanasonicMainView PanasonicMainView;

// Debounce delay for temperature changes (ms)
#define PANASONIC_SEND_DEBOUNCE_MS 800

typedef void (*PanasonicMainViewSendCallback)(void* context);
typedef void (*PanasonicMainViewNavigateCallback)(void* context);

PanasonicMainView* panasonic_main_view_alloc(void);
void panasonic_main_view_free(PanasonicMainView* view);
View* panasonic_main_view_get_view(PanasonicMainView* view);
void panasonic_main_view_set_state(PanasonicMainView* view, PanasonicState* state);

void panasonic_main_view_set_send_callback(
    PanasonicMainView* view,
    PanasonicMainViewSendCallback callback,
    void* context);
void panasonic_main_view_set_extra_callback(
    PanasonicMainView* view,
    PanasonicMainViewNavigateCallback callback,
    void* context);
void panasonic_main_view_set_setup_callback(
    PanasonicMainView* view,
    PanasonicMainViewNavigateCallback callback,
    void* context);

void panasonic_main_view_start_sending(PanasonicMainView* view);
void panasonic_main_view_update_sending(PanasonicMainView* view);
void panasonic_main_view_stop_sending(PanasonicMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int panasonic_main_view_get_last_command(PanasonicMainView* view, PanasonicToggle* out_toggle);

#ifdef __cplusplus
}
#endif
