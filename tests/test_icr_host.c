#include "morse_flipper_icr_host_test.h"

#include <assert.h>
#include <stdio.h>

static MorseFlipperMappedFalResult tick_result;
static unsigned lock_depth;
static unsigned backs;
static unsigned back_lock_depth;
static unsigned applies;
static unsigned redraws;
static unsigned sidetones;

void furi_mutex_acquire(FuriMutex* mutex, uint32_t timeout) {
    (void)mutex;
    (void)timeout;
    lock_depth++;
}

void furi_mutex_release(FuriMutex* mutex) {
    (void)mutex;
    assert(lock_depth == 1U);
    lock_depth--;
}

bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    uint8_t owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial) {
    (void)app;
    (void)owner;
    (void)mode;
    (void)path;
    (void)api_version;
    (void)api_magic;
    (void)minimum_api_size;
    (void)enter_args;
    (void)initial;
    return false;
}

bool morse_flipper_plugin_runtime_tick_locked(
    MorseFlipperApp* app,
    uint8_t owner,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result) {
    (void)app;
    (void)owner;
    (void)now_ms;
    *result = tick_result;
    return true;
}

void morse_flipper_plugin_runtime_apply_result_locked(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms) {
    (void)app;
    (void)result;
    (void)now_ms;
    applies++;
}

void morse_flipper_plugin_feedback_expire_locked(MorseFlipperApp* app, uint32_t now_ms) {
    (void)app;
    (void)now_ms;
}

void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    (void)app;
    sidetones++;
}

void morse_flipper_view_dirty(MorseFlipperApp* app) {
    (void)app;
    redraws++;
}

void morse_flipper_scene_back(MorseFlipperApp* app) {
    (void)app;
    backs++;
    back_lock_depth = lock_depth;
}

void morse_flipper_icr_host_tick(MorseFlipperApp* app, uint32_t now_ms);

int main(void) {
    FuriMutex mutex = {0};
    MorseFlipperMappedFalApi api = {0};
    MorseFlipperApp app = {
        .plugin_slot = {
            .mutex = &mutex,
            .api = &api,
            .state = &app,
            .owner = MorseFlipperPluginOwnerIcr,
        },
    };

    tick_result = (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
    morse_flipper_icr_host_tick(&app, 100U);
    assert(applies == 1U && redraws == 1U && sidetones == 1U && backs == 0U);

    tick_result = (MorseFlipperMappedFalResult){.handled = true, .request_exit = true};
    morse_flipper_icr_host_tick(&app, 200U);
    assert(applies == 2U && backs == 1U);
    assert(back_lock_depth == 0U);

    puts("test_icr_host: passed");
    return 0;
}
