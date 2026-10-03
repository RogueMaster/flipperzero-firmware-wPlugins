#pragma once

#include <gui/view.h>
#include <furi.h>
#include "../goodweather_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GoodweatherMainView GoodweatherMainView;

// Debounce delay for temperature changes (ms)
#define GOODWEATHER_SEND_DEBOUNCE_MS 800

typedef void (*GoodweatherMainViewSendCallback)(void* context);
typedef void (*GoodweatherMainViewNavigateCallback)(void* context);

GoodweatherMainView* goodweather_main_view_alloc(void);
void goodweather_main_view_free(GoodweatherMainView* view);
View* goodweather_main_view_get_view(GoodweatherMainView* view);
void goodweather_main_view_set_state(GoodweatherMainView* view, GoodweatherState* state);

void goodweather_main_view_set_send_callback(
    GoodweatherMainView* view,
    GoodweatherMainViewSendCallback callback,
    void* context);
void goodweather_main_view_set_extra_callback(
    GoodweatherMainView* view,
    GoodweatherMainViewNavigateCallback callback,
    void* context);
void goodweather_main_view_set_setup_callback(
    GoodweatherMainView* view,
    GoodweatherMainViewNavigateCallback callback,
    void* context);

void goodweather_main_view_start_sending(GoodweatherMainView* view);
void goodweather_main_view_update_sending(GoodweatherMainView* view);
void goodweather_main_view_stop_sending(GoodweatherMainView* view);

/**
 * What the last button press asked for.
 * @param out_toggle receives the toggle when the result is 1
 * @return 0 for a state command, 1 for a one-shot toggle
 */
int goodweather_main_view_get_last_command(
    GoodweatherMainView* view,
    GoodweatherToggle* out_toggle);

#ifdef __cplusplus
}
#endif
