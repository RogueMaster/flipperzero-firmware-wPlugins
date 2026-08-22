#pragma once

#include <gui/view.h>
#include "../daikin_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DaikinSetupView DaikinSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
DaikinSetupView* daikin_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void daikin_setup_view_free(DaikinSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* daikin_setup_view_get_view(DaikinSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void daikin_setup_view_set_state(DaikinSetupView* view, DaikinState* state);

#ifdef __cplusplus
}
#endif
