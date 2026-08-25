#pragma once

#include "../api_caller.h"

/** Start sending a saved call; response body lines are collected asynchronously. */
bool call_runner_start(AppContext* app, const CallEntry* call);

/**
 * Poll the in-flight request. Returns true when the request is over
 * (check app->call_error to distinguish success from failure).
 */
bool call_runner_poll(AppContext* app);
