#pragma once

// App is defined as an anonymous-struct typedef in app_user.h, so a
// forward `typedef struct App App;` would declare an incompatible type.
// Include app_user.h directly — there is no cycle (app_user.h does not
// include settings.h).
#include "../../app_user.h"

/**
 * Load persisted settings from /ext/apps_data/ethernet/settings.cfg into the
 * App. If the file is missing, malformed, or has a version mismatch, leaves
 * the App's in-memory defaults intact and returns silently. Never shows a
 * dialog or blocks the caller.
 *
 * Must be called only after the App has been fully constructed (storage
 * record open, ethernet allocated) and after make_paths() so the
 * containing directory exists.
 */
void settings_load(App* app);

/**
 * Persist the current App settings to /ext/apps_data/ethernet/settings.cfg.
 * Errors are silent — a failed write does not propagate.
 *
 * Must be called before app_free() begins tearing down storage records or
 * the ethernet instance.
 */
void settings_save(App* app);
