#pragma once

#include <gui/view.h>
#include "../mitsubishi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MitsubishiSetupView MitsubishiSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
MitsubishiSetupView* mitsubishi_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void mitsubishi_setup_view_free(MitsubishiSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* mitsubishi_setup_view_get_view(MitsubishiSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void mitsubishi_setup_view_set_state(MitsubishiSetupView* view, MitsubishiState* state);

#ifdef __cplusplus
}
#endif
