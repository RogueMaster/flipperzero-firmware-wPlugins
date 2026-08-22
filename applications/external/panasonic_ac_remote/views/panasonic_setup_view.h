#pragma once

#include <gui/view.h>
#include "../panasonic_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PanasonicSetupView PanasonicSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
PanasonicSetupView* panasonic_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void panasonic_setup_view_free(PanasonicSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* panasonic_setup_view_get_view(PanasonicSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void panasonic_setup_view_set_state(PanasonicSetupView* view, PanasonicState* state);

#ifdef __cplusplus
}
#endif
