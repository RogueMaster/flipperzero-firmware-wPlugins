#pragma once

#include <gui/view.h>
#include "../samsung_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SamsungSetupView SamsungSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
SamsungSetupView* samsung_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void samsung_setup_view_free(SamsungSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* samsung_setup_view_get_view(SamsungSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void samsung_setup_view_set_state(SamsungSetupView* view, SamsungState* state);

#ifdef __cplusplus
}
#endif
