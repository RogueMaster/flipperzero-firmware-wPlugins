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

#pragma once

#include <furi.h>
#if ZF_DEV_SCREENSHOT
#include <furi/core/pubsub.h>
#endif
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#include "u2f/session.h"
#include "transport/adapter.h"
#ifndef ZF_USB_ONLY
#include "transport/nfc_worker.h"
#endif
#ifndef ZF_NFC_ONLY
#include "transport/usb_hid_session.h"
#endif
#include "zerofido_pin.h"
#include "zerofido_runtime_config.h"
#include "zerofido_telemetry.h"
#include "zerofido_types.h"
#include "zerofido_ui_format.h"

typedef enum {
    ZfViewStatus = 0,
    ZfViewApproval = 1,
    ZfViewCredentials = 2,
    ZfViewCredentialDetail = 3,
    ZfViewSettings = 4,
    ZfViewPinMenu = 5,
    ZfViewPinInput = 6,
    ZfViewPinConfirm = 7,
    ZfViewCount = 8,
} ZfViewId;

typedef enum {
    ZfEventShowApproval = 1,
    ZfEventHideApproval,
    ZfEventConnected,
    ZfEventDisconnected,
    ZfEventActivity,
    ZfEventApprovalTimeout,
    ZfEventNotificationTimeout,
#if ZF_DEV_SCREENSHOT
    ZfEventDevScreenshot,
#endif
} ZfCustomEvent;

typedef enum {
    ZfApprovalIdle,
    ZfApprovalPending,
    ZfApprovalApproved,
    ZfApprovalDenied,
    ZfApprovalCanceled,
    ZfApprovalTimedOut,
} ZfApprovalState;

typedef enum {
    ZfInteractionKindApproval = 0,
    ZfInteractionKindAssertionSelection = 1,
} ZfInteractionKind;

typedef enum {
    ZfPinInputNone,
    ZfPinInputSetNew,
    ZfPinInputSetConfirm,
    ZfPinInputChangeCurrent,
    ZfPinInputChangeNew,
    ZfPinInputChangeConfirm,
    ZfPinInputRemoveCurrent,
} ZfPinInputState;

typedef enum {
    ZfPinConfirmActionNone,
    ZfPinConfirmActionRemove,
    ZfPinConfirmActionResume,
    ZfPinConfirmActionResetAppData,
    ZfPinConfirmActionDeleteCredential,
} ZfPinConfirmAction;

typedef struct {
    char input[64];
    char new_pin[64];
    char current[64];
} ZfPinBuffers;

/*
 * App state aggregates all long-lived subsystem handles. Transport workers and
 * UI callbacks synchronize through ui_mutex; command/UI scratch arenas are
 * wiped on release to keep protocol stacks small.
 */
typedef struct {
    ZfUiProtocol protocol;
    char operation[24];
    char user_text[ZF_MAX_USER_NAME_LEN + ZF_MAX_DISPLAY_NAME_LEN + 8];
} ZfApprovalPrompt;

typedef struct {
    uint16_t credential_indices[ZF_MAX_CREDENTIALS];
    uint8_t credential_count;
    uint16_t selected_menu_index;
    uint16_t selected_record_index;
} ZfAssertionSelectionPrompt;

typedef struct {
    ZfApprovalState state;
    ZfInteractionKind kind;
    char target_id[ZF_MAX_RP_ID_LEN];
    uint32_t generation;
    uint32_t pending_hide_generation;
    uint32_t deadline;
    FuriSemaphore *done;
    union {
        ZfApprovalPrompt approval;
        ZfAssertionSelectionPrompt selection;
    } details;
} ZfApprovalRequest;

typedef struct ZerofidoApp {
    Gui *gui;
    ViewDispatcher *view_dispatcher;
    View *status_view;
    Submenu *credentials_menu;
    Submenu *settings_menu;
    Submenu *pin_menu;
    TextInput *pin_input_view;
    DialogEx *pin_confirm_view;
    View *credential_detail_view;
    DialogEx *approval_view;
    Storage *storage;
    NotificationApp *notifications;
    FuriTimer *notify_timer;
    FuriThread *startup_thread;
    FuriThread *worker_thread;
    FuriMutex *ui_mutex;
#if !defined(ZF_USB_ONLY) && !defined(ZF_NFC_ONLY)
    const ZfTransportAdapterOps *transport_adapter;
#endif
#ifndef ZF_NFC_ONLY
    FuriHalUsbInterface *previous_usb;
#endif
    void *transport_state;
#ifndef ZF_NFC_ONLY
    ZfTransportState transport_state_storage;
#endif
#if defined(ZF_HOST_TEST) && !defined(ZF_USB_ONLY)
    ZfNfcTransportState transport_nfc_state_storage;
#elif !defined(ZF_USB_ONLY)
    ZfNfcTransportState *transport_nfc_state_storage;
#endif
    U2fData *u2f;
    ZfClientPinState pin_state;
    ZfRuntimeConfig runtime_config;
    ZfResolvedCapabilities capabilities;
    bool capabilities_resolved;
    bool running;
    bool ui_events_enabled;
    bool transport_connected;
    bool maintenance_busy;
    bool transport_auto_accept_transaction;
    bool startup_complete;
    bool startup_ok;
    bool status_refresh_pending;
    bool status_credentials_dirty;
#if ZF_RELEASE_DIAGNOSTICS
    uint32_t telemetry_next_idle_tick;
#endif
#if ZF_DEV_SCREENSHOT
    FuriPubSub *dev_screenshot_input_events;
    FuriPubSubSubscription *dev_screenshot_input_subscription;
    uint8_t dev_screenshot_frame[1024];
    size_t dev_screenshot_frame_size;
    uint32_t dev_screenshot_counter;
    bool dev_screenshot_frame_valid;
    bool dev_screenshot_framebuffer_registered;
#endif
    uint32_t ui_registered_views;
    ZfViewId active_view;
    FuriThreadId ui_thread_id;
    char status_text[64];
    uint8_t *transport_arena;
    size_t transport_arena_size;
    ZfCommandScratchArena *command_scratch;
    size_t command_scratch_size;
    bool command_scratch_in_use;
    ZfUiScratchArena *ui_scratch;
    ZfPinBuffers *pin_buffers;
    uint32_t credentials_selected_index;
    uint32_t settings_selected_index;
    uint32_t pin_menu_selected_index;
    ZfPinInputState pin_input_state;
    ZfPinConfirmAction pin_confirm_action;
    ZfViewId pin_confirm_return_view;
    bool startup_reset_available;
    bool store_records_owned;
    ZfApprovalRequest approval;
    ZfCredentialStore store;
    ZfAssertionQueue assertion_queue;
} ZerofidoApp;

#ifndef ZF_USB_ONLY
static inline ZfNfcTransportState *zf_app_nfc_transport_state(ZerofidoApp *app) {
    if (!app) {
        return NULL;
    }
#ifdef ZF_HOST_TEST
    return &app->transport_nfc_state_storage;
#else
    return app->transport_nfc_state_storage;
#endif
}

static inline void zf_app_set_nfc_transport_state(ZerofidoApp *app, ZfNfcTransportState *state) {
    if (!app) {
        return;
    }
#ifdef ZF_HOST_TEST
    (void)state;
#else
    app->transport_nfc_state_storage = state;
#endif
}
#endif

/* Transport scratch is allocated only while a transport worker/session needs it. */
static inline bool zf_app_transport_arena_acquire(ZerofidoApp *app) {
    if (!app) {
        return false;
    }
    if (app->transport_arena) {
        return app->transport_arena_size >= ZF_TRANSPORT_ARENA_SIZE;
    }

    app->transport_arena = malloc(ZF_TRANSPORT_ARENA_SIZE);
    if (!app->transport_arena) {
        app->transport_arena_size = 0U;
        zf_telemetry_log_oom("transport arena", ZF_TRANSPORT_ARENA_SIZE);
        return false;
    }

    app->transport_arena_size = ZF_TRANSPORT_ARENA_SIZE;
    memset(app->transport_arena, 0, app->transport_arena_size);
    return true;
}

static inline size_t zf_app_transport_arena_capacity(const ZerofidoApp *app) {
    return app && app->transport_arena ? app->transport_arena_size : 0U;
}

static inline void zf_app_transport_arena_wipe(ZerofidoApp *app) {
    if (!app || !app->transport_arena) {
        return;
    }

    volatile uint8_t *bytes = app->transport_arena;
    for (size_t i = 0; i < app->transport_arena_size; ++i) {
        bytes[i] = 0;
    }
}

static inline void zf_app_transport_arena_release(ZerofidoApp *app) {
    if (!app || !app->transport_arena) {
        return;
    }

    zf_app_transport_arena_wipe(app);
    free(app->transport_arena);
    app->transport_arena = NULL;
    app->transport_arena_size = 0U;
}

/* Command scratch is single-owner per operation and allocated only while active. */
static inline void *zf_app_command_scratch_acquire(ZerofidoApp *app, size_t size) {
    if (!app || size > ZF_COMMAND_SCRATCH_SIZE || app->command_scratch_in_use ||
        app->command_scratch) {
        return NULL;
    }

    app->command_scratch = malloc(sizeof(*app->command_scratch));
    if (!app->command_scratch) {
        app->command_scratch_size = 0U;
        zf_telemetry_log_oom("command scratch", sizeof(*app->command_scratch));
        return NULL;
    }

    memset(app->command_scratch, 0, sizeof(*app->command_scratch));
    app->command_scratch_size = size;
    app->command_scratch_in_use = true;
    return app->command_scratch->bytes;
}

static inline void zf_app_command_scratch_release(ZerofidoApp *app) {
    if (!app || !app->command_scratch) {
        return;
    }

    volatile uint8_t *bytes = app->command_scratch->bytes;
    for (size_t i = 0; i < sizeof(app->command_scratch->bytes); ++i) {
        bytes[i] = 0;
    }
    free(app->command_scratch);
    app->command_scratch = NULL;
    app->command_scratch_in_use = false;
    app->command_scratch_size = 0;
}

static inline void zf_app_command_scratch_destroy(ZerofidoApp *app) {
    if (!app) {
        return;
    }

    if (app->command_scratch_in_use) {
        zf_app_command_scratch_release(app);
    }
    if (app->command_scratch) {
        volatile uint8_t *bytes = app->command_scratch->bytes;
        for (size_t i = 0; i < sizeof(app->command_scratch->bytes); ++i) {
            bytes[i] = 0;
        }
        free(app->command_scratch);
        app->command_scratch = NULL;
    }
    app->command_scratch_size = 0;
    app->command_scratch_in_use = false;
}

/* UI scratch is separate from command scratch because view callbacks run on the UI thread. */
static inline void *zf_app_ui_scratch_acquire(ZerofidoApp *app, size_t size) {
    if (!app || size > ZF_UI_SCRATCH_SIZE || app->ui_scratch) {
        return NULL;
    }

    app->ui_scratch = malloc(sizeof(*app->ui_scratch));
    if (!app->ui_scratch) {
        zf_telemetry_log_oom("ui scratch", sizeof(*app->ui_scratch));
        return NULL;
    }

    memset(app->ui_scratch, 0, sizeof(*app->ui_scratch));
    return app->ui_scratch->bytes;
}

static inline void zf_app_ui_scratch_release(ZerofidoApp *app) {
    if (!app || !app->ui_scratch) {
        return;
    }

    volatile uint8_t *bytes = app->ui_scratch->bytes;
    for (size_t i = 0; i < sizeof(app->ui_scratch->bytes); ++i) {
        bytes[i] = 0;
    }
    free(app->ui_scratch);
    app->ui_scratch = NULL;
}
