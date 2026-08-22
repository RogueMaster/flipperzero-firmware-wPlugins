#pragma once

#include <gui/view.h>
#include "../toshiba_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ToshibaSetupView ToshibaSetupView;

/**
 * Allocate setup view
 * @return Allocated view
 */
ToshibaSetupView* toshiba_setup_view_alloc(void);

/**
 * Free setup view
 * @param view View to free
 */
void toshiba_setup_view_free(ToshibaSetupView* view);

/**
 * Get underlying View
 * @param view Setup view
 * @return View pointer for ViewDispatcher
 */
View* toshiba_setup_view_get_view(ToshibaSetupView* view);

/**
 * Set state reference
 * @param view Setup view
 * @param state State pointer
 */
void toshiba_setup_view_set_state(ToshibaSetupView* view, ToshibaState* state);

#ifdef __cplusplus
}
#endif
