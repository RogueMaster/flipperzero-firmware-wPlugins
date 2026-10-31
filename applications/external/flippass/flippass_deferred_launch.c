#include "flippass.h"
#include "flippass_deferred_launch.h"
#include "kdbx/memzero.h"

#include <core/kernel.h>
#include <loader/loader.h>
#include <stdlib.h>
#include <string.h>

static void flippass_deferred_launch_free(FlipPassDeferredLaunch* launch) {
    if(launch != NULL) {
        memzero(launch, launch->allocation_bytes);
        free(launch);
    }
}

void flippass_deferred_launch_cancel(App* app) {
    flippass_deferred_launch_free(app->deferred_launch);
    app->deferred_launch = NULL;
}

bool flippass_deferred_launch_prepare(
    App* app,
    const char* application,
    const char* arguments,
    bool self) {
    if(app->database_dirty || app->deferred_launch != NULL || application == NULL ||
       application[0] == '\0' || arguments == NULL)
        return false;
    FuriString* launch_path = NULL;
    if(self) {
        launch_path = furi_string_alloc();
        Loader* loader = furi_record_open(RECORD_LOADER);
        const bool found = loader_get_application_launch_path(loader, launch_path);
        furi_record_close(RECORD_LOADER);
        if(!found || furi_string_empty(launch_path)) {
            furi_string_free(launch_path);
            return false;
        }
        application = furi_string_get_cstr(launch_path);
    }
    const size_t name_bytes = strlen(application) + 1U;
    const size_t args_bytes = strlen(arguments) + 1U;
    if(name_bytes > 1024U || args_bytes > 4096U) {
        if(launch_path != NULL) furi_string_free(launch_path);
        return false;
    }
    const size_t bytes = sizeof(FlipPassDeferredLaunch) + name_bytes + args_bytes;
    const int32_t lock = furi_kernel_lock();
    FlipPassDeferredLaunch* launch =
        memmgr_heap_get_max_free_block() >= bytes + 16U ? malloc(bytes) : NULL;
    furi_kernel_restore_lock(lock);
    if(launch != NULL) {
        launch->allocation_bytes = bytes;
        launch->committed = false;
        launch->application = launch->data;
        launch->arguments = launch->data + name_bytes;
        memcpy(launch->data, application, name_bytes);
        memcpy(launch->data + name_bytes, arguments, args_bytes);
        app->deferred_launch = launch;
    }
    if(launch_path != NULL) furi_string_free(launch_path);
    return launch != NULL;
}

void flippass_deferred_launch_finish_output(App* app, bool success) {
    if(app->deferred_launch == NULL) return;
    if(!success || app->database_dirty || flippass_typing_should_cancel(app)) {
        flippass_deferred_launch_cancel(app);
        return;
    }
    app->deferred_launch->committed = true;
    flippass_request_exit(app);
}

FlipPassDeferredLaunch* flippass_deferred_launch_take(App* app) {
    FlipPassDeferredLaunch* launch = app->deferred_launch;
    if(launch == NULL || !launch->committed) return NULL;
    app->deferred_launch = NULL;
    return launch;
}

void flippass_deferred_launch_submit(FlipPassDeferredLaunch* launch) {
    if(launch == NULL) return;
    Loader* loader = furi_record_open(RECORD_LOADER);
    /* Do not clear the native FIFO: unrelated queued requests retain order.
     * Firmware owns the copies after this synchronous enqueue returns. */
    loader_enqueue_launch(
        loader,
        launch->application,
        launch->arguments[0] != '\0' ? launch->arguments : NULL,
        LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);
    flippass_deferred_launch_free(launch);
}
