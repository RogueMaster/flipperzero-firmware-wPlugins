#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../delonghi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DelonghiMainView DelonghiMainView;

// Debounce delay for temperature changes (ms)
#define DELONGHI_SEND_DEBOUNCE_MS 800

typedef void (*DelonghiMainViewSendCallback)(void* context);
typedef void (*DelonghiMainViewNavigateCallback)(void* context);

DelonghiMainView* delonghi_main_view_alloc(void);
void delonghi_main_view_free(DelonghiMainView* view);
View* delonghi_main_view_get_view(DelonghiMainView* view);
void delonghi_main_view_set_state(DelonghiMainView* view, DelonghiState* state);

void delonghi_main_view_set_send_callback(
    DelonghiMainView* view,
    DelonghiMainViewSendCallback callback,
    void* context);
void delonghi_main_view_set_extra_callback(
    DelonghiMainView* view,
    DelonghiMainViewNavigateCallback callback,
    void* context);
void delonghi_main_view_set_setup_callback(
    DelonghiMainView* view,
    DelonghiMainViewNavigateCallback callback,
    void* context);

void delonghi_main_view_start_sending(DelonghiMainView* view);
void delonghi_main_view_update_sending(DelonghiMainView* view);
void delonghi_main_view_stop_sending(DelonghiMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int delonghi_main_view_get_last_command(DelonghiMainView* view, DelonghiToggle* out_toggle);

#ifdef __cplusplus
}
#endif
