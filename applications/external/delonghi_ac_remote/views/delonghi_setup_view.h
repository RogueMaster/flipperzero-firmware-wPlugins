#pragma once

#include <gui/view.h>
#include "../delonghi_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DelonghiSetupView DelonghiSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
DelonghiSetupView* delonghi_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void delonghi_setup_view_free(DelonghiSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* delonghi_setup_view_get_view(DelonghiSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void delonghi_setup_view_set_state(DelonghiSetupView* view, DelonghiState* state);

#ifdef __cplusplus
}
#endif
