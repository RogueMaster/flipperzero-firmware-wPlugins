#pragma once

#include <gui/view.h>
#include "../goodweather_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GoodweatherSetupView GoodweatherSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
GoodweatherSetupView* goodweather_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void goodweather_setup_view_free(GoodweatherSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* goodweather_setup_view_get_view(GoodweatherSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void goodweather_setup_view_set_state(GoodweatherSetupView* view, GoodweatherState* state);

#ifdef __cplusplus
}
#endif
