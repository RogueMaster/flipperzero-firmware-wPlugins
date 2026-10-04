#include "mjs_fal.h"
#include <furi.h>
#include <flipper_application/plugins/plugin_manager.h>

#define TAG "MjsFal"

typedef enum {
    MjsFalUnloaded,
    MjsFalLoading,
    MjsFalReady,
    MjsFalUnloading,
} MjsFalState;

static struct {
    FuriMutex* mutex;
    PluginManager* manager;
    const MjsFalApi* api;
    size_t references;
    MjsFalState state;
} engine;

/* Allocate outside the critical section. The mutex stays resident after first
 * use so another thread can never wait on a mutex being freed. */
static FuriMutex* mjs_fal_mutex(void) {
    FuriMutex* mutex;
    {
        FURI_CRITICAL_ENTER();
        mutex = engine.mutex;
        FURI_CRITICAL_EXIT();
    }
    if(!mutex) {
        FuriMutex* candidate = furi_mutex_alloc(FuriMutexTypeRecursive);
        {
            FURI_CRITICAL_ENTER();
            if(!engine.mutex) {
                engine.mutex = candidate;
                candidate = NULL;
            }
            mutex = engine.mutex;
            FURI_CRITICAL_EXIT();
        }
        if(candidate) furi_mutex_free(candidate);
    }
    return mutex;
}

static bool mjs_fal_api_valid(const MjsFalApi* api) {
    if(!api || api->size != sizeof(MjsFalApi) || !api->create || !api->destroy || !api->vcall ||
       !api->vset_errorf || !api->vprepend_errorf) {
        return false;
    }
#define MJS_FAL_CHECK(ret, name, params, args) \
    if(!api->name) return false;
    MJS_FAL_FUNCTIONS(MJS_FAL_CHECK)
#undef MJS_FAL_CHECK
    return true;
}

bool mjs_fal_acquire(void) {
    FuriMutex* mutex = mjs_fal_mutex();
    furi_check(furi_mutex_acquire(mutex, FuriWaitForever) == FuriStatusOk);
    bool success = false;

    if(engine.state == MjsFalUnloaded) {
        engine.state = MjsFalLoading;
        PluginManager* manager = plugin_manager_alloc(MJS_FAL_APP_ID, MJS_FAL_API_VERSION, NULL);
        if(plugin_manager_load_single(manager, MJS_FAL_PATH) == PluginManagerErrorNone) {
            const MjsFalApi* api = plugin_manager_get_ep(manager, 0);
            if(mjs_fal_api_valid(api)) {
                engine.manager = manager;
                __atomic_store_n(&engine.api, api, __ATOMIC_RELEASE);
                engine.state = MjsFalReady;
            }
        }
        if(engine.state != MjsFalReady) {
            /* Keep Loading set during destructors: recursive loads must fail. */
            plugin_manager_free(manager);
            engine.state = MjsFalUnloaded;
            FURI_LOG_E(
                TAG, "Cannot load %s; check matching SD resources and free memory", MJS_FAL_PATH);
        }
    }

    /* A recursive import during load/unload must fail without deadlocking.
     * In particular, the engine FAL must define all its own mJS symbols. */
    if(engine.state == MjsFalReady) {
        furi_check(engine.references != SIZE_MAX);
        engine.references++;
        success = true;
    }

    furi_check(furi_mutex_release(mutex) == FuriStatusOk);
    return success;
}

void mjs_fal_release(void) {
    FuriMutex* mutex = mjs_fal_mutex();
    furi_check(furi_mutex_acquire(mutex, FuriWaitForever) == FuriStatusOk);
    furi_check(engine.state == MjsFalReady && engine.references);
    if(--engine.references == 0) {
        engine.state = MjsFalUnloading;
        __atomic_store_n(&engine.api, NULL, __ATOMIC_RELEASE);
        plugin_manager_free(engine.manager);
        engine.manager = NULL;
        engine.state = MjsFalUnloaded;
    }
    furi_check(furi_mutex_release(mutex) == FuriStatusOk);
}

const MjsFalApi* mjs_fal_get_api(void) {
    /* The caller's context/import lease prevents unloading. Only publishing
     * the table needs ordering; ordinary SDK calls never take the loader lock. */
    const MjsFalApi* api = __atomic_load_n(&engine.api, __ATOMIC_ACQUIRE);
    furi_check(api);
    return api;
}
