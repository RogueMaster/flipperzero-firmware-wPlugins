#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../carrier_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CarrierMainView CarrierMainView;

// Debounce delay for temperature changes (ms)
#define CARRIER_SEND_DEBOUNCE_MS 800

typedef void (*CarrierMainViewSendCallback)(void* context);
typedef void (*CarrierMainViewNavigateCallback)(void* context);

CarrierMainView* carrier_main_view_alloc(void);
void carrier_main_view_free(CarrierMainView* view);
View* carrier_main_view_get_view(CarrierMainView* view);
void carrier_main_view_set_state(CarrierMainView* view, CarrierState* state);

void carrier_main_view_set_send_callback(
    CarrierMainView* view,
    CarrierMainViewSendCallback callback,
    void* context);
void carrier_main_view_set_extra_callback(
    CarrierMainView* view,
    CarrierMainViewNavigateCallback callback,
    void* context);
void carrier_main_view_set_setup_callback(
    CarrierMainView* view,
    CarrierMainViewNavigateCallback callback,
    void* context);

void carrier_main_view_start_sending(CarrierMainView* view);
void carrier_main_view_update_sending(CarrierMainView* view);
void carrier_main_view_stop_sending(CarrierMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int carrier_main_view_get_last_command(CarrierMainView* view, CarrierToggle* out_toggle);

#ifdef __cplusplus
}
#endif
