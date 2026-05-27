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

#include "../zerofido_ui.h"

#include <string.h>

#include "../transport/adapter.h"
#include "../zerofido_app_i.h"
#include "../zerofido_notify.h"
#include "../zerofido_runtime_config.h"
#include "../zerofido_ui_i.h"

/*
 * Approval terminal transitions are one-shot: only Pending can complete, and a
 * successful completion releases approval.done exactly once.
 */
static bool zerofido_ui_finish_interaction_locked(ZerofidoApp *app, ZfApprovalState state) {
    if (app->approval.state != ZfApprovalPending) {
        return false;
    }

    app->approval.state = state;
    app->approval.pending_hide_generation = app->approval.generation;
    furi_semaphore_release(app->approval.done);
    return true;
}

static void zerofido_dialog_result_callback(DialogExResult result, void *context) {
    ZerofidoApp *app = context;
    bool finished = false;
    furi_assert(app);

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    finished = zerofido_ui_finish_interaction_locked(
        app, result == DialogExResultRight ? ZfApprovalApproved : ZfApprovalDenied);
    furi_mutex_release(app->ui_mutex);
    if (finished) {
        zf_transport_notify_interaction_changed(app);
    }

    zerofido_ui_dispatch_custom_event(app, ZfEventHideApproval);
}

static void zerofido_ui_prepare_interaction_locked(ZerofidoApp *app, ZfInteractionKind kind,
                                                   ZfUiProtocol protocol, const char *operation,
                                                   const char *target_id, const char *user_text) {
    /*
     * generation and pending_hide_generation keep delayed hide events from
     * closing a newer prompt that reused the same view.
     */
    app->approval.generation++;
    app->approval.pending_hide_generation = 0;
    app->approval.state = ZfApprovalPending;
    app->approval.kind = kind;
    app->approval.deadline = furi_get_tick() + ZF_APPROVAL_TIMEOUT_MS;
    strncpy(app->approval.target_id, target_id ? target_id : "",
            sizeof(app->approval.target_id) - 1);
    app->approval.target_id[sizeof(app->approval.target_id) - 1] = '\0';
    memset(&app->approval.details, 0, sizeof(app->approval.details));

    if (kind == ZfInteractionKindApproval) {
        app->approval.details.approval.protocol = protocol;
        zf_ui_format_approval_header(app->approval.details.approval.operation,
                                     sizeof(app->approval.details.approval.operation), protocol,
                                     operation);
        app->approval.details.approval
            .operation[sizeof(app->approval.details.approval.operation) - 1] = '\0';
        strncpy(app->approval.details.approval.user_text, user_text ? user_text : "",
                sizeof(app->approval.details.approval.user_text) - 1);
        app->approval.details.approval
            .user_text[sizeof(app->approval.details.approval.user_text) - 1] = '\0';
    } else {
        app->approval.details.selection.selected_menu_index = UINT16_MAX;
        app->approval.details.selection.selected_record_index = UINT16_MAX;
    }
}

static void zerofido_ui_clear_interaction_signal(ZerofidoApp *app) {
    while (furi_semaphore_acquire(app->approval.done, 0) == FuriStatusOk) {
    }
}

static bool zerofido_ui_wait_for_interaction_result(ZerofidoApp *app,
                                                    ZfTransportSessionId current_session_id,
                                                    bool *approved) {
    if (!zf_transport_wait_for_interaction(app, current_session_id, approved)) {
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (approved) {
        *approved = (app->approval.state == ZfApprovalApproved);
    }
    bool ok =
        app->approval.state == ZfApprovalApproved || app->approval.state == ZfApprovalDenied ||
        app->approval.state == ZfApprovalCanceled || app->approval.state == ZfApprovalTimedOut;
    furi_mutex_release(app->ui_mutex);
    return ok;
}

/*
 * Requests a transport-facing approval. The return value says whether the
 * interaction reached a terminal state; approved separately says whether that
 * terminal state was approval.
 */
bool zerofido_ui_request_approval(ZerofidoApp *app, ZfUiProtocol protocol, const char *operation,
                                  const char *target_id, const char *user_text,
                                  ZfTransportSessionId current_session_id, bool *approved) {
#if ZF_AUTO_ACCEPT_REQUESTS
    ZfResolvedCapabilities capabilities;
#endif

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (!app->ui_events_enabled || !app->running || app->maintenance_busy) {
        furi_mutex_release(app->ui_mutex);
        if (approved) {
            *approved = false;
        }
        return false;
    }

#if ZF_AUTO_ACCEPT_REQUESTS
    zf_runtime_get_effective_capabilities(app, &capabilities);
    if (capabilities.auto_accept_requests || app->transport_auto_accept_transaction) {
#else
    if (app->transport_auto_accept_transaction) {
#endif
        furi_mutex_release(app->ui_mutex);
        if (approved) {
            *approved = true;
        }
        return true;
    }
    zerofido_ui_prepare_interaction_locked(app, ZfInteractionKindApproval, protocol, operation,
                                           target_id, user_text);
    zerofido_ui_clear_interaction_signal(app);
    furi_mutex_release(app->ui_mutex);

    zerofido_ui_dispatch_custom_event(app, ZfEventShowApproval);
    return zerofido_ui_wait_for_interaction_result(app, current_session_id, approved);
}

bool zerofido_ui_request_assertion_selection(ZerofidoApp *app, const char *rp_id,
                                             const uint16_t *match_indices, size_t match_count,
                                             ZfTransportSessionId current_session_id,
                                             uint32_t *selected_record_index) {
    bool approved = false;

    if (!match_indices || match_count == 0 || match_count > ZF_MAX_CREDENTIALS ||
        !selected_record_index) {
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (!app->ui_events_enabled || !app->running || app->maintenance_busy) {
        furi_mutex_release(app->ui_mutex);
        return false;
    }

    zerofido_ui_prepare_interaction_locked(app, ZfInteractionKindAssertionSelection,
                                           ZfUiProtocolFido2, "Select account", rp_id,
                                           "Choose account");
    app->approval.details.selection.credential_count = (uint8_t)match_count;
    for (size_t i = 0; i < match_count; ++i) {
        app->approval.details.selection.credential_indices[i] = match_indices[i];
    }
    zerofido_ui_clear_interaction_signal(app);
    furi_mutex_release(app->ui_mutex);

    zerofido_ui_dispatch_custom_event(app, ZfEventShowApproval);
    if (!zerofido_ui_wait_for_interaction_result(app, current_session_id, &approved) || !approved) {
        return false;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    *selected_record_index = app->approval.details.selection.selected_record_index;
    furi_mutex_release(app->ui_mutex);
    return *selected_record_index != UINT16_MAX;
}

void zerofido_ui_show_interaction(ZerofidoApp *app) {
    char body[160];
    ZfInteractionKind kind = ZfInteractionKindApproval;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    kind = app->approval.kind;
    if (kind == ZfInteractionKindApproval) {
        zf_ui_format_approval_body(body, sizeof(body), app->approval.details.approval.protocol,
                                   app->approval.target_id,
                                   app->approval.details.approval.user_text);
    }
    furi_mutex_release(app->ui_mutex);

    if (kind == ZfInteractionKindApproval) {
        if (!zerofido_ui_ensure_view(app, ZfViewApproval)) {
            return;
        }
        dialog_ex_reset(app->approval_view);
        dialog_ex_set_context(app->approval_view, app);
        dialog_ex_set_result_callback(app->approval_view, zerofido_dialog_result_callback);
        dialog_ex_set_header(app->approval_view, app->approval.details.approval.operation, 64, 0,
                             AlignCenter, AlignTop);
        dialog_ex_set_text(app->approval_view, body, 64, 28, AlignCenter, AlignCenter);
        dialog_ex_set_left_button_text(app->approval_view, "Deny");
        dialog_ex_set_center_button_text(app->approval_view, NULL);
        dialog_ex_set_right_button_text(app->approval_view, "Approve");
    }

    zerofido_notify_prompt(app);
}

void zerofido_ui_hide_interaction(ZerofidoApp *app) {
    ZfApprovalState approval_state;
    ZfInteractionKind kind;
    uint32_t generation = 0;
    bool should_hide = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    approval_state = app->approval.state;
    kind = app->approval.kind;
    generation = app->approval.generation;
    should_hide =
        app->approval.pending_hide_generation == generation && approval_state != ZfApprovalPending;
    if (should_hide) {
        app->approval.pending_hide_generation = 0;
    }
    furi_mutex_release(app->ui_mutex);
    if (!should_hide) {
        return;
    }

    if (kind == ZfInteractionKindApproval && app->approval_view) {
        dialog_ex_reset(app->approval_view);
    }
    zerofido_ui_switch_to_view(app, ZfViewStatus);
    if (approval_state == ZfApprovalTimedOut) {
        zerofido_ui_set_status(app, "Request timed out");
        zerofido_notify_error(app);
    } else if (approval_state == ZfApprovalDenied) {
        zerofido_ui_set_status(app, "Request denied");
        zerofido_notify_error(app);
    } else {
        zerofido_ui_refresh_status(app);
        zerofido_notify_reset(app);
    }
}

bool zerofido_ui_deny_pending_interaction(ZerofidoApp *app) {
    bool denied = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    denied = zerofido_ui_finish_interaction_locked(app, ZfApprovalDenied);
    furi_mutex_release(app->ui_mutex);
    if (denied) {
        zf_transport_notify_interaction_changed(app);
    }
    return denied;
}

bool zerofido_ui_expire_pending_interaction(ZerofidoApp *app) {
    bool timed_out = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->approval.state == ZfApprovalPending &&
        ((int32_t)(furi_get_tick() - app->approval.deadline) >= 0)) {
        timed_out = zerofido_ui_finish_interaction_locked(app, ZfApprovalTimedOut);
    }
    furi_mutex_release(app->ui_mutex);
    if (timed_out) {
        zf_transport_notify_interaction_changed(app);
    }
    return timed_out;
}

bool zerofido_ui_cancel_pending_interaction(ZerofidoApp *app) {
    bool canceled = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    canceled = zerofido_ui_cancel_pending_interaction_locked(app);
    furi_mutex_release(app->ui_mutex);
    if (canceled) {
        zf_transport_notify_interaction_changed(app);
    }
    return canceled;
}

bool zerofido_ui_cancel_pending_interaction_locked(ZerofidoApp *app) {
    return zerofido_ui_finish_interaction_locked(app, ZfApprovalCanceled);
}

ZfApprovalState zerofido_ui_get_interaction_state(ZerofidoApp *app) {
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    ZfApprovalState state = app->approval.state;
    furi_mutex_release(app->ui_mutex);
    return state;
}
