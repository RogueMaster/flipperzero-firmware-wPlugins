#pragma once

#include <gui/view.h>
#include "../carrier_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CarrierSetupView CarrierSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
CarrierSetupView* carrier_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void carrier_setup_view_free(CarrierSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* carrier_setup_view_get_view(CarrierSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void carrier_setup_view_set_state(CarrierSetupView* view, CarrierState* state);

#ifdef __cplusplus
}
#endif
