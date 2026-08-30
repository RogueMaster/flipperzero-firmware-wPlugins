#pragma once

#include "seos_sm_command.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Turns a secure messaging event into a view dispatcher event.
 *
 * Every transport wants exactly this, and `context` is the Seos* each already
 * passes as its event context. Declared apart from seos_i.h so the plugins and
 * the app's symbol table can see it without dragging the GUI headers in.
 */
void seos_sm_event_to_view_dispatcher(void* context, SeosSmEvent event);

#ifdef __cplusplus
}
#endif
