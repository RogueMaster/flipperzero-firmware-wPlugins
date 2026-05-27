/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "lifecycle.h"

#include <string.h>

#include "../transport/adapter.h"
#if defined(ZF_USB_ONLY)
#include "../transport/usb_hid_worker.h"
#elif defined(ZF_NFC_ONLY)
#include "../transport/nfc_worker.h"
#endif
#include "../u2f/adapter.h"
#include "../zerofido_attestation.h"
#include "../zerofido_crypto.h"
#include "../zerofido_notify.h"
#include "../zerofido_pin.h"
#include "../zerofido_runtime_config.h"
#include "../zerofido_store.h"
#include "../zerofido_telemetry.h"
#include "../zerofido_ui.h"
#include "../zerofido_ui_i.h"
#include "../zerofido_usb_diagnostics.h"

typedef enum {
    ZfStorageInitOk = 0,
    ZfStorageInitFailed,
    ZfStorageInitInvalidPinState,
} ZfStorageInitStatus;

static void zf_app_lifecycle_close_records(ZerofidoApp *app);

static bool zf_app_lifecycle_open_records(ZerofidoApp *app) {
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->ui_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->approval.done = furi_semaphore_alloc(1, 0);

    if (!(app->gui && app->storage && app->notifications && app->ui_mutex && app->approval.done)) {
        zf_app_lifecycle_close_records(app);
        return false;
    }

    if (!zerofido_notify_init(app)) {
        zf_app_lifecycle_close_records(app);
        return false;
    }
    return true;
}

static void zf_app_lifecycle_close_records(ZerofidoApp *app) {
    if (!app) {
        return;
    }

    if (app->approval.done) {
        furi_semaphore_free(app->approval.done);
        app->approval.done = NULL;
    }
    if (app->ui_mutex) {
        furi_mutex_free(app->ui_mutex);
        app->ui_mutex = NULL;
    }
    if (app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }
    if (app->notifications) {
        zerofido_notify_deinit(app);
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }
    if (app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }
}

static ZfStorageInitStatus zf_app_lifecycle_init_storage(ZerofidoApp *app) {
    ZfPinInitResult pin_init = ZfPinInitOk;
    ZfCredentialIndexEntry *existing_records = app->store.records;
    ZfResolvedCapabilities capabilities;
    uint8_t *store_io = NULL;

    zf_attestation_reset_consistency_cache();
#if ZF_USB_DIAGNOSTICS
    zf_usb_diag_reset(app->storage);
    zf_usb_diag_log(app->storage, "startup");
#endif
    zerofido_ui_set_status(app, "Key");
    if (!zf_crypto_ensure_store_key()) {
        return ZfStorageInitFailed;
    }
    store_io = malloc(ZF_STORE_RECORD_IO_SIZE);
    if (!store_io) {
        zf_telemetry_log_oom("startup store io", ZF_STORE_RECORD_IO_SIZE);
        return ZfStorageInitFailed;
    }
    zerofido_ui_set_status(app, "Index");
    if (!zf_store_init_with_buffer(app->storage, &app->store, store_io, ZF_STORE_RECORD_IO_SIZE)) {
        zf_crypto_secure_zero(store_io, ZF_STORE_RECORD_IO_SIZE);
        free(store_io);
        if (!existing_records && app->store.records) {
            zf_crypto_secure_zero(app->store.records,
                                  app->store.capacity * sizeof(app->store.records[0]));
            free(app->store.records);
            app->store.records = NULL;
            app->store.capacity = 0U;
            app->store.count = 0U;
        }
        return ZfStorageInitFailed;
    }
    if (!existing_records && app->store.records) {
        app->store_records_owned = true;
    }
    zf_crypto_secure_zero(store_io, ZF_STORE_RECORD_IO_SIZE);
    free(store_io);

    zerofido_ui_set_status(app, "PIN");
    pin_init = zerofido_pin_init_with_result(app->storage, &app->pin_state);
    if (pin_init == ZfPinInitInvalidPersistedState) {
        return ZfStorageInitInvalidPinState;
    }
    if (pin_init != ZfPinInitOk) {
        return ZfStorageInitFailed;
    }
    zf_runtime_config_refresh_capabilities(app);
    zf_runtime_get_effective_capabilities(app, &capabilities);

#if ZF_PACKED_ATTESTATION
    if (capabilities.attestation_mode == ZfAttestationModePacked) {
        zerofido_ui_set_status(app, "Attest");
        if (!zf_attestation_ensure_ready()) {
            zf_telemetry_log("attestation prewarm failed");
        }
    }
#endif
    if (capabilities.u2f_enabled) {
        zerofido_ui_set_status(app, "U2F keys");
        if (!zf_u2f_adapter_init(app)) {
            zf_telemetry_log("u2f key prewarm failed");
        }
    }

    return ZfStorageInitOk;
}

#if !defined(ZF_USB_ONLY) && !defined(ZF_NFC_ONLY)
static const ZfTransportAdapterOps *zf_app_lifecycle_adapter_for_mode(ZfTransportMode mode) {
    switch (mode) {
    case ZfTransportModeNfc:
        return &zf_transport_nfc_adapter;
    case ZfTransportModeUsbHid:
    default:
        return &zf_transport_usb_hid_adapter;
    }
}
#endif

static void zf_app_lifecycle_load_runtime_config(ZerofidoApp *app) {
    ZfRuntimeConfig runtime_config;

    zf_runtime_config_load(app->storage, &runtime_config);
    zf_runtime_config_apply(app, &runtime_config);
#if !defined(ZF_USB_ONLY) && !defined(ZF_NFC_ONLY)
    app->transport_adapter = zf_app_lifecycle_adapter_for_mode(app->runtime_config.transport_mode);
#endif
}

static const char *zf_app_lifecycle_backend_status(const ZerofidoApp *app,
                                                   ZfStorageInitStatus storage_status,
                                                   bool u2f_ready) {
    ZfResolvedCapabilities capabilities;
    bool transport_usable = false;

    zf_runtime_get_effective_capabilities(app, &capabilities);
    transport_usable = (capabilities.fido2_enabled || !capabilities.u2f_enabled || u2f_ready);
    if (storage_status == ZfStorageInitOk && transport_usable) {
        return NULL;
    }
    if (storage_status == ZfStorageInitInvalidPinState) {
        return "PIN state invalid";
    }
    if (storage_status != ZfStorageInitOk && !u2f_ready) {
        return "Backend init failed";
    }
    return storage_status == ZfStorageInitOk ? "U2F unavailable" : "Storage init failed";
}

static bool zf_app_lifecycle_start_worker(ZerofidoApp *app) {
    FuriThread *thread = NULL;
    size_t worker_stack_size = 8 * 1024;

    if (!app) {
        return false;
    }

#if defined(ZF_USB_ONLY)
    int32_t (*worker)(void *) = zf_transport_usb_hid_worker;
    worker_stack_size = 6 * 1024;
#elif defined(ZF_NFC_ONLY)
    int32_t (*worker)(void *) = zf_transport_nfc_worker;
    worker_stack_size = 4 * 1024;
#else
    if (!app->transport_adapter || !app->transport_adapter->worker) {
        return false;
    }
    if (app->transport_adapter->worker_stack_size > 0) {
        worker_stack_size = app->transport_adapter->worker_stack_size;
    }
#endif

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->worker_thread) {
        furi_mutex_release(app->ui_mutex);
        return true;
    }
    furi_mutex_release(app->ui_mutex);

#if defined(ZF_USB_ONLY) || defined(ZF_NFC_ONLY)
    thread = furi_thread_alloc_ex("ZeroFIDOWorker", worker_stack_size, worker, app);
#else
    thread = furi_thread_alloc_ex("ZeroFIDOWorker", worker_stack_size,
                                  app->transport_adapter->worker, app);
#endif
    if (!thread) {
        zf_telemetry_log_oom("worker thread alloc", worker_stack_size);
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->worker_thread) {
        furi_mutex_release(app->ui_mutex);
        furi_thread_free(thread);
        return true;
    }
    app->worker_thread = thread;
    furi_mutex_release(app->ui_mutex);

    furi_thread_set_appid(thread, ZF_APP_ID);
    furi_thread_start(thread);
    return true;
}

static void zf_app_lifecycle_stop_worker(ZerofidoApp *app) {
    FuriThread *worker_thread = NULL;

    if (!app || !app->ui_mutex) {
        return;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    worker_thread = app->worker_thread;
    furi_mutex_release(app->ui_mutex);
    if (!worker_thread) {
        return;
    }

    zf_transport_stop(app);
    furi_thread_join(worker_thread);
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->worker_thread == worker_thread) {
        app->worker_thread = NULL;
    }
    furi_mutex_release(app->ui_mutex);
    furi_thread_free(worker_thread);
}

ZerofidoApp *zf_app_lifecycle_alloc(void) {
    ZerofidoApp *app = malloc(sizeof(ZerofidoApp));
    if (!app) {
        zf_telemetry_log_oom("lifecycle alloc", sizeof(ZerofidoApp));
        return NULL;
    }

    memset(app, 0, sizeof(*app));
    app->running = true;
    app->ui_events_enabled = true;
    zf_runtime_config_load_defaults(&app->runtime_config);
#if !defined(ZF_USB_ONLY) && !defined(ZF_NFC_ONLY)
    app->transport_adapter = zf_app_lifecycle_adapter_for_mode(app->runtime_config.transport_mode);
#endif
    zf_runtime_config_apply(app, &app->runtime_config);
    return app;
}

bool zf_app_lifecycle_open(ZerofidoApp *app) {
    if (!zf_app_lifecycle_open_records(app)) {
        return false;
    }

    zf_app_lifecycle_load_runtime_config(app);
    return zerofido_ui_init(app);
}

bool zf_app_lifecycle_startup(ZerofidoApp *app) {
    zf_telemetry_log("startup begin");
    zerofido_ui_set_status(app, "Config");
    zf_app_lifecycle_load_runtime_config(app);

    zerofido_ui_set_status(app, "Storage");
    ZfStorageInitStatus storage_status = zf_app_lifecycle_init_storage(app);
    bool u2f_ready = !app->capabilities.u2f_enabled || zf_u2f_adapter_is_available(app);
    const char *backend_status = zf_app_lifecycle_backend_status(app, storage_status, u2f_ready);

    if (backend_status) {
        app->startup_reset_available = storage_status == ZfStorageInitInvalidPinState;
        zerofido_ui_set_status(app, backend_status);
    } else {
        zerofido_ui_set_status(app, NULL);
    }

    zerofido_ui_refresh_status_line(app);
    zerofido_ui_refresh_credentials_status(app);
    zf_telemetry_log("startup end");
    return backend_status == NULL;
}

static int32_t zf_app_lifecycle_startup_worker(void *context) {
    ZerofidoApp *app = context;

    zf_telemetry_log("startup worker start");
    bool ok = zf_app_lifecycle_startup(app);

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->startup_ok = ok;
    app->startup_complete = true;
    furi_mutex_release(app->ui_mutex);

    if (!ok) {
        zerofido_ui_set_status(app, "Startup failed");
    } else {
        zerofido_ui_set_status(app, NULL);
    }
    zerofido_ui_refresh_credentials_status(app);
    zf_telemetry_log(ok ? "startup worker ok" : "startup worker failed");
    return 0;
}

bool zf_app_lifecycle_startup_async(ZerofidoApp *app) {
    if (!app || !app->ui_mutex) {
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->startup_complete = false;
    app->startup_ok = false;
    furi_mutex_release(app->ui_mutex);

    zerofido_ui_set_status(app, NULL);
    app->startup_thread =
        furi_thread_alloc_ex("ZeroFIDOStart", 8 * 1024, zf_app_lifecycle_startup_worker, app);
    if (!app->startup_thread) {
        zf_telemetry_log_oom("startup thread alloc", 8 * 1024);
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        app->startup_ok = false;
        app->startup_complete = true;
        furi_mutex_release(app->ui_mutex);
        zerofido_ui_set_status(app, "Startup failed");
        return false;
    }

    furi_thread_set_appid(app->startup_thread, ZF_APP_ID);
    furi_thread_start(app->startup_thread);
    return true;
}

void zf_app_lifecycle_wait_startup(ZerofidoApp *app) {
    FuriThread *startup_thread = NULL;

    if (!app || !app->ui_mutex) {
        return;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    startup_thread = app->startup_thread;
    app->startup_thread = NULL;
    furi_mutex_release(app->ui_mutex);

    if (!startup_thread) {
        return;
    }

    furi_thread_join(startup_thread);
    furi_thread_free(startup_thread);
}

/*
 * Startup thread ownership has two states while startup_thread is non-NULL:
 * running, or completed-but-not-joined. startup_complete distinguishes them and
 * gates the join plus transport-worker start.
 */
bool zf_app_lifecycle_startup_pending(ZerofidoApp *app) {
    FuriThread *completed_thread = NULL;
    bool pending = false;
    bool completed_ok = false;
    bool start_transport = false;

    if (!app || !app->ui_mutex) {
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->startup_thread && app->startup_complete) {
        completed_thread = app->startup_thread;
        app->startup_thread = NULL;
        completed_ok = app->startup_ok;
        start_transport = completed_ok && app->running && !app->worker_thread &&
                          (app->capabilities.fido2_enabled || app->capabilities.u2f_enabled);
    } else {
        pending = app->startup_thread != NULL;
    }
    furi_mutex_release(app->ui_mutex);

    if (completed_thread) {
        furi_thread_join(completed_thread);
        furi_thread_free(completed_thread);
        if (start_transport) {
            zerofido_ui_set_status(app, "Transport");
            if (!zf_app_lifecycle_start_worker(app)) {
                furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
                app->startup_ok = false;
                furi_mutex_release(app->ui_mutex);
                zerofido_ui_set_status(app, "Transport init failed");
                zerofido_ui_refresh_status_line(app);
                return false;
            }
        }
        if (completed_ok) {
            zerofido_ui_set_status(app, NULL);
        }
        zerofido_ui_refresh_credentials_status(app);
    }
    return pending;
}

bool zf_app_lifecycle_restart_transport(ZerofidoApp *app) {
    if (!app) {
        return false;
    }
    if (zf_app_lifecycle_startup_pending(app)) {
        return false;
    }

    zf_app_lifecycle_stop_worker(app);
#if !defined(ZF_USB_ONLY) && !defined(ZF_NFC_ONLY)
    app->transport_adapter = zf_app_lifecycle_adapter_for_mode(app->runtime_config.transport_mode);
#endif
    if (!(app->capabilities.fido2_enabled || app->capabilities.u2f_enabled)) {
        return true;
    }

    return zf_app_lifecycle_start_worker(app);
}

bool zf_app_lifecycle_set_transport_mode(ZerofidoApp *app, Storage *storage, ZfTransportMode mode) {
    ZfRuntimeConfig previous_config;
    ZfRuntimeConfig next_config;

    if (!app || (mode != ZfTransportModeUsbHid && mode != ZfTransportModeNfc)) {
        return false;
    }
#if defined(ZF_USB_ONLY)
    if (mode != ZfTransportModeUsbHid) {
        return false;
    }
#elif defined(ZF_NFC_ONLY)
    if (mode != ZfTransportModeNfc) {
        return false;
    }
#endif

    previous_config = app->runtime_config;
    if (previous_config.transport_mode == mode) {
        return true;
    }

    next_config = previous_config;
    next_config.transport_mode = mode;
    zf_runtime_config_apply(app, &next_config);
    if (!zf_app_lifecycle_restart_transport(app)) {
        zf_runtime_config_apply(app, &previous_config);
        (void)zf_app_lifecycle_restart_transport(app);
        return false;
    }

    if (!zf_runtime_config_persist(storage, &next_config)) {
        zf_runtime_config_apply(app, &previous_config);
        (void)zf_app_lifecycle_restart_transport(app);
        return false;
    }

    return true;
}

void zf_app_lifecycle_shutdown(ZerofidoApp *app) {
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->running = false;
    app->ui_events_enabled = false;
    furi_mutex_release(app->ui_mutex);
    zf_app_lifecycle_wait_startup(app);
    zf_app_lifecycle_stop_worker(app);
}

void zf_app_lifecycle_free(ZerofidoApp *app) {
    if (!app) {
        return;
    }

    ZfCredentialIndexEntry *owned_records = app->store_records_owned ? app->store.records : NULL;

    zf_u2f_adapter_deinit(app);
    zf_store_deinit(&app->store);
    if (owned_records) {
        zf_crypto_secure_zero(owned_records, sizeof(owned_records[0]) * app->store.capacity);
        free(owned_records);
        app->store.records = NULL;
        app->store.capacity = 0U;
        app->store_records_owned = false;
    }
    zf_app_command_scratch_destroy(app);
    zf_app_transport_arena_release(app);
    zerofido_ui_deinit(app);
    zf_app_lifecycle_close_records(app);
    zf_crypto_secure_zero(app, sizeof(*app));
    free(app);
}
