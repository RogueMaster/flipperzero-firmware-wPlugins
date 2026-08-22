#pragma once

#include <gui/view.h>
#include "../mitsubishi_heavy_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MitsubishiHeavySetupView MitsubishiHeavySetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
MitsubishiHeavySetupView* mitsubishi_heavy_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void mitsubishi_heavy_setup_view_free(MitsubishiHeavySetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* mitsubishi_heavy_setup_view_get_view(MitsubishiHeavySetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void mitsubishi_heavy_setup_view_set_state(
    MitsubishiHeavySetupView* view,
    MitsubishiHeavyState* state);

#ifdef __cplusplus
}
#endif
