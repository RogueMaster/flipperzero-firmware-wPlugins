#pragma once

#include <gui/view.h>
#include "../kelvinator_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KelvinatorSetupView KelvinatorSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
KelvinatorSetupView* kelvinator_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void kelvinator_setup_view_free(KelvinatorSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* kelvinator_setup_view_get_view(KelvinatorSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void kelvinator_setup_view_set_state(KelvinatorSetupView* view, KelvinatorState* state);

#ifdef __cplusplus
}
#endif
