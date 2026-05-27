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

#include "approval.h"

#include "../policy.h"
#include "../../zerofido_app_i.h"
#include "../../zerofido_ui.h"

/* Maps terminal UI interaction states to CTAP status codes. */
static uint8_t zf_ctap_status_from_interaction_state(ZfApprovalState state,
                                                     bool timeout_is_denied) {
    switch (state) {
    case ZfApprovalTimedOut:
        return timeout_is_denied ? ZF_CTAP_ERR_OPERATION_DENIED : ZF_CTAP_ERR_USER_ACTION_TIMEOUT;
    case ZfApprovalCanceled:
        return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
    case ZfApprovalDenied:
    case ZfApprovalIdle:
    case ZfApprovalPending:
    case ZfApprovalApproved:
    default:
        return ZF_CTAP_ERR_OPERATION_DENIED;
    }
}

/* Requests a single approve/deny interaction from the UI layer. */
uint8_t zf_ctap_request_approval(ZerofidoApp *app, const char *operation, const char *rp_id,
                                 const char *user_text, ZfTransportSessionId session_id) {
    bool approved = false;
    if (!zerofido_ui_request_approval(app, ZfUiProtocolFido2, operation, rp_id, user_text,
                                      session_id, &approved)) {
        return ZF_CTAP_ERR_USER_ACTION_TIMEOUT;
    }
    if (approved) {
        return ZF_CTAP_SUCCESS;
    }

    return zf_ctap_status_from_interaction_state(zerofido_ui_get_interaction_state(app), false);
}

/* Requests account selection for multi-credential getAssertion results. */
uint8_t zf_ctap_request_assertion_selection(ZerofidoApp *app, const char *rp_id,
                                            const uint16_t *match_indices, size_t match_count,
                                            ZfTransportSessionId session_id,
                                            uint32_t *selected_record_index) {
    if (!zerofido_ui_request_assertion_selection(app, rp_id, match_indices, match_count, session_id,
                                                 selected_record_index)) {
        return zf_ctap_status_from_interaction_state(zerofido_ui_get_interaction_state(app), true);
    }

    return ZF_CTAP_SUCCESS;
}

/*
 * CTAP requires empty pinAuth probes to wait for touch before returning the PIN
 * state error, which prevents silent probing without user presence.
 */
uint8_t zf_ctap_handle_empty_pin_auth_probe(ZerofidoApp *app, ZfTransportSessionId session_id,
                                            const char *operation, const char *rp_id,
                                            const char *user_text) {
    uint8_t status = zf_ctap_request_approval(app, operation, rp_id, user_text, session_id);
    if (status != ZF_CTAP_SUCCESS) {
        return status;
    }

    return zf_ctap_pin_is_set(app) ? ZF_CTAP_ERR_PIN_INVALID : ZF_CTAP_ERR_PIN_NOT_SET;
}
