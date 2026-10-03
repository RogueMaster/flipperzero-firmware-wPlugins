#pragma once

#include <gui/view.h>
#include "../kelon_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KelonSetupView KelonSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
KelonSetupView* kelon_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void kelon_setup_view_free(KelonSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* kelon_setup_view_get_view(KelonSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void kelon_setup_view_set_state(KelonSetupView* view, KelonState* state);

#ifdef __cplusplus
}
#endif
