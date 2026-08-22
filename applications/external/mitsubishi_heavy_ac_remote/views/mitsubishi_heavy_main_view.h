#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../mitsubishi_heavy_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MitsubishiHeavyMainView MitsubishiHeavyMainView;

// Debounce delay for temperature changes (ms)
#define MITSUBISHI_HEAVY_SEND_DEBOUNCE_MS 800

typedef void (*MitsubishiHeavyMainViewSendCallback)(void* context);
typedef void (*MitsubishiHeavyMainViewNavigateCallback)(void* context);

MitsubishiHeavyMainView* mitsubishi_heavy_main_view_alloc(void);
void mitsubishi_heavy_main_view_free(MitsubishiHeavyMainView* view);
View* mitsubishi_heavy_main_view_get_view(MitsubishiHeavyMainView* view);
void mitsubishi_heavy_main_view_set_state(
    MitsubishiHeavyMainView* view,
    MitsubishiHeavyState* state);

void mitsubishi_heavy_main_view_set_send_callback(
    MitsubishiHeavyMainView* view,
    MitsubishiHeavyMainViewSendCallback callback,
    void* context);
void mitsubishi_heavy_main_view_set_extra_callback(
    MitsubishiHeavyMainView* view,
    MitsubishiHeavyMainViewNavigateCallback callback,
    void* context);
void mitsubishi_heavy_main_view_set_setup_callback(
    MitsubishiHeavyMainView* view,
    MitsubishiHeavyMainViewNavigateCallback callback,
    void* context);

void mitsubishi_heavy_main_view_start_sending(MitsubishiHeavyMainView* view);
void mitsubishi_heavy_main_view_update_sending(MitsubishiHeavyMainView* view);
void mitsubishi_heavy_main_view_stop_sending(MitsubishiHeavyMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int mitsubishi_heavy_main_view_get_last_command(
    MitsubishiHeavyMainView* view,
    MitsubishiHeavyToggle* out_toggle);

#ifdef __cplusplus
}
#endif
