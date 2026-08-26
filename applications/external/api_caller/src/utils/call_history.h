#pragma once

#include <furi.h>

#include "../api_caller.h"

#define CALL_HISTORY_FILETYPE "ApiCaller Call History"
#define CALL_HISTORY_VERSION  1

/** Load the saved calls into app->call_history (count is updated). */
void call_history_load(AppContext* app);

/**
 * Append app->call_form to the history and persist it.
 * Returns false if the list is full or the storage write fails.
 */
bool call_history_add(AppContext* app);

/**
 * Copy app->call_form into the entry at index and persist the history.
 * Returns false if the index is invalid or the storage write fails.
 */
bool call_history_update(AppContext* app, uint8_t index);

/** Remove the entry at index (shifting the rest) and persist the history. */
bool call_history_remove(AppContext* app, uint8_t index);
