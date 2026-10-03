#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../mitsubishi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MitsubishiMainView MitsubishiMainView;

// Debounce delay for temperature changes (ms)
#define MITSUBISHI_SEND_DEBOUNCE_MS 800

typedef void (*MitsubishiMainViewSendCallback)(void* context);
typedef void (*MitsubishiMainViewNavigateCallback)(void* context);

MitsubishiMainView* mitsubishi_main_view_alloc(void);
void mitsubishi_main_view_free(MitsubishiMainView* view);
View* mitsubishi_main_view_get_view(MitsubishiMainView* view);
void mitsubishi_main_view_set_state(MitsubishiMainView* view, MitsubishiState* state);

void mitsubishi_main_view_set_send_callback(
    MitsubishiMainView* view,
    MitsubishiMainViewSendCallback callback,
    void* context);
void mitsubishi_main_view_set_extra_callback(
    MitsubishiMainView* view,
    MitsubishiMainViewNavigateCallback callback,
    void* context);
void mitsubishi_main_view_set_setup_callback(
    MitsubishiMainView* view,
    MitsubishiMainViewNavigateCallback callback,
    void* context);

void mitsubishi_main_view_start_sending(MitsubishiMainView* view);
void mitsubishi_main_view_update_sending(MitsubishiMainView* view);
void mitsubishi_main_view_stop_sending(MitsubishiMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int mitsubishi_main_view_get_last_command(MitsubishiMainView* view, MitsubishiToggle* out_toggle);

#ifdef __cplusplus
}
#endif
