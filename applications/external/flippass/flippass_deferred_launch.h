#pragma once

#include <stdbool.h>
#include <stddef.h>

struct App;
typedef struct FlipPassDeferredLaunch {
    size_t allocation_bytes;
    const char* application;
    const char* arguments;
    bool committed;
    char data[];
} FlipPassDeferredLaunch;

bool flippass_deferred_launch_prepare(
    struct App* app,
    const char* application,
    const char* arguments,
    bool self);
void flippass_deferred_launch_cancel(struct App* app);
void flippass_deferred_launch_finish_output(struct App* app, bool success);
FlipPassDeferredLaunch* flippass_deferred_launch_take(struct App* app);
/* Called only after App and all its plugins/views have been freed. The native
 * loader runs the copied request after joining and unloading this FAP. */
void flippass_deferred_launch_submit(FlipPassDeferredLaunch* launch);
