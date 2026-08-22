#pragma once

#include <gui/view.h>
#include "../coolix_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoolixSetupView CoolixSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
CoolixSetupView* coolix_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void coolix_setup_view_free(CoolixSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* coolix_setup_view_get_view(CoolixSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void coolix_setup_view_set_state(CoolixSetupView* view, CoolixState* state);

#ifdef __cplusplus
}
#endif
