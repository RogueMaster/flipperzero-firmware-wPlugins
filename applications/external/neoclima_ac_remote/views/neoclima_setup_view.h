#pragma once

#include <gui/view.h>
#include "../neoclima_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NeoclimaSetupView NeoclimaSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
NeoclimaSetupView* neoclima_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void neoclima_setup_view_free(NeoclimaSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* neoclima_setup_view_get_view(NeoclimaSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void neoclima_setup_view_set_state(NeoclimaSetupView* view, NeoclimaState* state);

#ifdef __cplusplus
}
#endif
