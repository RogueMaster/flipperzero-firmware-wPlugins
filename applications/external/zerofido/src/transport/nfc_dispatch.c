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

#ifndef ZF_USB_ONLY

#include "nfc_dispatch.h"

#include <stdio.h>
#include <string.h>

#include "../zerofido_app_i.h"
#include "../zerofido_attestation.h"
#include "../zerofido_crypto.h"
#include "../zerofido_ctap.h"
#include "../zerofido_pin.h"
#include "../zerofido_runtime_config.h"
#include "../zerofido_ui.h"
#include "../ctap/response.h"
#include "../u2f/apdu.h"
#include "../u2f/adapter.h"
#include "../u2f/common.h"
#include "nfc_iso_dep.h"
#include "nfc_protocol.h"
#include "nfc_session.h"
#include "nfc_trace.h"

#define ZF_NFC_DIAG_EVENT(text) zf_transport_nfc_trace_event((text))

#define ZF_NFC_DESFIRE_SW_MORE 0x91AFU
#define ZF_NFC_DESFIRE_SW_OK 0x9100U
#if ZF_RELEASE_DIAGNOSTICS
#define ZF_NFC_CTAP_DIAG(...) zf_transport_nfc_trace_format("ctap " __VA_ARGS__)

static const char *zf_transport_nfc_ctap_status_name(uint8_t status) {
    switch (status) {
    case ZF_CTAP_SUCCESS:
        return "OK";
    case ZF_CTAP_ERR_INVALID_CBOR:
        return "CBOR";
    case ZF_CTAP_ERR_OTHER:
        return "OTHER";
    default:
        return "UNK";
    }
}
#else
#define ZF_NFC_CTAP_DIAG(...)                                                                      \
    do {                                                                                           \
    } while (false)
#endif

static bool zf_transport_nfc_send_desfire_version_part(ZerofidoApp *app, ZfNfcTransportState *state,
                                                       bool final_frame) {
    static const uint8_t version_part1_or_2[] = {
        0x04, 0x01, 0x01, 0x01, 0x00, 0x18, 0x05,
    };
    static const uint8_t version_part3[] = {
        0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x00, 0x00, 0x00, 0x00, 0x24, 0x04, 0x26,
    };

    if (!state) {
        return false;
    }

    (void)app;
    ZF_NFC_DIAG_EVENT(final_frame ? "DESFire version done" : "DESFire version");
    if (final_frame) {
        /* Shim-local discovery progress only; do not touch FIDO applet/session state. */
        state->desfire_probe_frame = 0U;
        return zf_transport_nfc_send_apdu_payload_preserving_replay(
            state, version_part3, sizeof(version_part3), ZF_NFC_DESFIRE_SW_OK);
    }

    /* Shim-local discovery progress only; do not touch FIDO applet/session state. */
    state->desfire_probe_frame++;
    return zf_transport_nfc_send_apdu_payload_preserving_replay(
        state, version_part1_or_2, sizeof(version_part1_or_2), ZF_NFC_DESFIRE_SW_MORE);
}

static bool zf_transport_nfc_send_desfire_version_terminal(ZerofidoApp *app,
                                                           ZfNfcTransportState *state) {
    static const uint8_t version_full[] = {
        0x04, 0x01, 0x01, 0x01, 0x00, 0x18, 0x05, 0x04, 0x01, 0x01, 0x01, 0x00, 0x18, 0x05,
        0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x00, 0x00, 0x00, 0x00, 0x24, 0x04, 0x26,
    };

    if (!state) {
        return false;
    }

    (void)app;
    ZF_NFC_DIAG_EVENT("DESFire version done");
    /* Shim-local discovery progress only; do not touch FIDO applet/session state. */
    state->desfire_probe_frame = 0U;
    return zf_transport_nfc_send_apdu_payload_preserving_replay(
        state, version_full, sizeof(version_full), ZF_NFC_DESFIRE_SW_OK);
}

static bool zf_transport_nfc_exchange_busy(const ZfNfcTransportState *state) {
    return state && (state->processing || state->request_pending || state->response_ready ||
                     state->iso4_tx_chain_active || zf_transport_nfc_request_worker_active(state));
}

static bool zf_transport_nfc_ctap_msg_p1p2_valid(const ZfNfcApdu *apdu) {
    return apdu && (apdu->p1 & (uint8_t)~ZF_NFC_CTAP_MSG_P1_GET_RESPONSE) == 0U && apdu->p2 == 0U;
}

static bool zf_transport_nfc_ctap_msg_get_response_supported(const ZfNfcApdu *apdu) {
    return apdu && (apdu->p1 & ZF_NFC_CTAP_MSG_P1_GET_RESPONSE) != 0U;
}

static bool zf_transport_nfc_send_busy_status(ZfNfcTransportState *state) {
    return zf_transport_nfc_send_apdu_payload(state, (const uint8_t[]){ZF_NFC_STATUS_PROCESSING},
                                              1U, ZF_NFC_SW_STATUS_UPDATE);
}

__attribute__((noinline)) static bool
zf_transport_nfc_append_ctap_chained_fragment(ZerofidoApp *app, ZfNfcTransportState *state,
                                              const ZfNfcApdu *apdu) {
    if (!app || !state || !apdu || (!apdu->data && apdu->data_len != 0U)) {
        return false;
    }

    if (state->request_len > ZF_MAX_MSG_SIZE ||
        apdu->data_len > ZF_MAX_MSG_SIZE - state->request_len) {
        zf_transport_nfc_reset_exchange_locked(state);
        return false;
    }
    if (apdu->data_len != 0U) {
        memmove(&app->transport_arena[state->request_len], apdu->data, apdu->data_len);
    }
    state->request_len += apdu->data_len;
    state->ctap_get_response_supported = state->ctap_get_response_supported ||
                                         zf_transport_nfc_ctap_msg_get_response_supported(apdu);
    return true;
}

static bool zf_transport_nfc_begin_ctap_msg(ZerofidoApp *app, ZfNfcTransportState *state,
                                            const ZfNfcApdu *apdu) {
    if (!app || !state || !apdu || !apdu->data) {
        return false;
    }
    if (apdu->data_len == 0U || apdu->data_len > ZF_MAX_MSG_SIZE) {
        return false;
    }
    memmove(app->transport_arena, apdu->data, apdu->data_len);
    state->request_len = apdu->data_len;
    state->ctap_get_response_supported = zf_transport_nfc_ctap_msg_get_response_supported(apdu);
    return true;
}

static void zf_transport_nfc_force_get_info_nfc_capabilities(ZfResolvedCapabilities *capabilities) {
    if (!capabilities) {
        return;
    }

    capabilities->nfc_enabled = true;
    capabilities->advertise_nfc_transport = true;
    capabilities->usb_hid_enabled = false;
    capabilities->advertise_usb_transport = false;
}

static bool zf_transport_nfc_pin_is_set_snapshot(ZerofidoApp *app, bool caller_holds_ui_mutex) {
    bool client_pin_set = false;

    if (!app) {
        return false;
    }
    if (!caller_holds_ui_mutex) {
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    }
    client_pin_set = zerofido_pin_is_set(&app->pin_state);
    if (!caller_holds_ui_mutex) {
        furi_mutex_release(app->ui_mutex);
    }
    return client_pin_set;
}

static bool zf_transport_nfc_send_get_info_response(ZerofidoApp *app, ZfNfcTransportState *state,
                                                    const ZfResolvedCapabilities *base_capabilities,
                                                    bool caller_holds_ui_mutex) {
    ZfResolvedCapabilities capabilities;
    uint8_t *response = NULL;
    size_t response_capacity = 0U;
    size_t body_capacity = 0U;
    size_t body_len = 0U;
    size_t response_len = 0U;
    bool client_pin_set = false;
    uint8_t status = ZF_CTAP_ERR_OTHER;

    if (!app || !state) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
    }

    response = zf_transport_nfc_arena(state);
    response_capacity = zf_transport_nfc_arena_capacity(state);
    if (!response || response_capacity <= 1U) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
    }
    body_capacity = response_capacity - 1U;

    if (base_capabilities) {
        capabilities = *base_capabilities;
    } else {
        zf_runtime_get_effective_capabilities(app, &capabilities);
    }
    zf_transport_nfc_force_get_info_nfc_capabilities(&capabilities);
    client_pin_set = zf_transport_nfc_pin_is_set_snapshot(app, caller_holds_ui_mutex);

    ZF_NFC_CTAP_DIAG("start cmd=GI len=1");
    ZF_NFC_CTAP_DIAG("dispatch cmd=GI");
    ZF_NFC_CTAP_DIAG("GI caps f21=%u pin=%u p2=%u token=%u mcu=%u rk=1 nfc=%u",
                     capabilities.advertise_fido_2_1 ? 1U : 0U, client_pin_set ? 1U : 0U,
                     capabilities.pin_uv_auth_protocol_2_enabled ? 1U : 0U,
                     capabilities.pin_uv_auth_token_enabled ? 1U : 0U,
                     capabilities.make_cred_uv_not_required ? 1U : 0U,
                     capabilities.advertise_nfc_transport ? 1U : 0U);
    status = zf_ctap_build_get_info_response(&capabilities, client_pin_set, response + 1U,
                                             body_capacity, &body_len);
    if (status != ZF_CTAP_SUCCESS) {
        body_len = 0U;
    }
    response[0] = status;
    response_len = body_len + 1U;
    ZF_NFC_CTAP_DIAG("done cmd=GI status=%s body=%u", zf_transport_nfc_ctap_status_name(status),
                     (unsigned)body_len);

    if (response_len + 2U <= ZF_NFC_MAX_TX_FRAME_INF_SIZE) {
        return zf_transport_nfc_send_apdu_payload(state, response, response_len, ZF_NFC_SW_SUCCESS);
    }
    return zf_transport_nfc_begin_chained_apdu_payload(state, response, response_len,
                                                       ZF_NFC_SW_SUCCESS);
}

static bool zf_transport_nfc_send_ctap2_direct(ZerofidoApp *app, ZfNfcTransportState *state,
                                               const uint8_t *request, size_t request_len,
                                               bool caller_holds_ui_mutex,
                                               bool request_was_extended_apdu) {
    size_t response_len = 0U;
    uint8_t *response = NULL;
    uint8_t request_copy[ZF_MAX_MSG_SIZE];
    size_t response_capacity = 0U;
    bool old_auto_accept = false;

    if (!app || !state || !request || request_len == 0U || request_len > ZF_MAX_MSG_SIZE) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
    }
    memset(request_copy, 0, sizeof(request_copy));

    if (!caller_holds_ui_mutex) {
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    }
    memcpy(request_copy, request, request_len);
    old_auto_accept = app->transport_auto_accept_transaction;
    app->transport_auto_accept_transaction = true;
    state->processing = true;
    state->processing_cancel_requested = false;
    state->processing_session_id = state->session_id;
    state->request_kind = ZfNfcRequestKindCtap2;
    zf_transport_nfc_attach_arena(state, NULL, 0U);
    zf_app_transport_arena_release(app);
    if (zf_app_transport_arena_acquire(app)) {
        response = app->transport_arena;
        response_capacity = zf_app_transport_arena_capacity(app);
        memset(response, 0, response_capacity);
        zf_transport_nfc_attach_arena(state, response, response_capacity);
    }
    furi_mutex_release(app->ui_mutex);

    if (response && response_capacity > 1U) {
        response_len = zerofido_handle_ctap2(
            app, state->session_id, request_copy, request_len, response,
            response_capacity > ZF_MAX_MSG_SIZE ? ZF_MAX_MSG_SIZE : response_capacity);
        zerofido_ui_refresh_status(app);
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    app->transport_auto_accept_transaction = old_auto_accept;
    state->processing = false;
    state->processing_session_id = 0U;
    state->request_kind = ZfNfcRequestKindNone;
    if (!caller_holds_ui_mutex) {
        furi_mutex_release(app->ui_mutex);
    }
    zf_crypto_secure_zero(request_copy, sizeof(request_copy));

    if (response_len == 0U) {
        zf_app_transport_arena_release(app);
        zf_transport_nfc_attach_arena(state, NULL, 0U);
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
    }
    if (response_len + 2U <= ZF_NFC_MAX_TX_FRAME_INF_SIZE) {
        const bool sent =
            zf_transport_nfc_send_apdu_payload(state, response, response_len, ZF_NFC_SW_SUCCESS);
        zf_app_transport_arena_release(app);
        zf_transport_nfc_attach_arena(state, NULL, 0U);
        return sent;
    }

    if (request_was_extended_apdu) {
        ZF_NFC_DIAG_EVENT("CTAP2 direct ext");
        return zf_transport_nfc_begin_chained_apdu_payload(state, response, response_len,
                                                           ZF_NFC_SW_SUCCESS);
    }

    state->response_len = response_len;
    state->response_offset = 0U;
    state->response_ready = true;
    state->response_is_u2f = false;
    state->response_is_error = false;
    state->error_status_word = 0U;
    state->processing = false;
    state->request_pending = false;
    state->request_kind = ZfNfcRequestKindNone;
    state->processing_session_id = 0U;
    state->command_chain_active = false;
    ZF_NFC_DIAG_EVENT("CTAP2 direct GR");

    const ZfNfcApdu get_response_apdu = {
        .cla = 0x00U,
        .ins = ZF_NFC_INS_ISO_GET_RESPONSE,
        .p1 = 0x00U,
        .p2 = 0x00U,
        .le = 0U,
        .has_le = true,
    };
    return zf_transport_nfc_send_get_response(app, state, &get_response_apdu);
}

static bool zf_transport_nfc_send_u2f_response(const ZerofidoApp *app, ZfNfcTransportState *state,
                                               const ZfNfcApdu *apdu, const uint8_t *response,
                                               size_t response_len) {
    uint16_t status_word = ZF_NFC_SW_INTERNAL_ERROR;
    uint8_t *arena = NULL;
    size_t arena_capacity = 0U;

    if (!app || !state || !apdu || !response || response_len < 2U) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
    }

    status_word = ((uint16_t)response[response_len - 2U] << 8) | response[response_len - 1U];
    if (response_len <= ZF_NFC_MAX_TX_FRAME_INF_SIZE) {
        return zf_transport_nfc_send_apdu_payload(state, response, response_len - 2U, status_word);
    }

    arena = zf_transport_nfc_arena(state);
    arena_capacity = zf_transport_nfc_arena_capacity(state);
    if (!arena || response_len > arena_capacity) {
        zf_transport_nfc_reset_exchange_locked(state);
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
    }
    if (response != arena) {
        memcpy(arena, response, response_len);
    }

    state->response_len = response_len;
    state->response_offset = 0U;
    state->response_ready = true;
    state->response_is_u2f = true;
    state->response_is_error = false;
    state->error_status_word = 0U;
    state->processing = false;
    state->request_pending = false;
    state->request_kind = ZfNfcRequestKindNone;
    state->processing_session_id = 0U;
    state->command_chain_active = false;
    state->ctap_get_response_supported = false;

    return zf_transport_nfc_send_get_response(app, state, apdu);
}

static inline size_t zf_transport_nfc_run_u2f_adapter(ZerofidoApp *app, ZfNfcTransportState *state,
                                                      size_t u2f_request_len,
                                                      const char *success_diag,
                                                      const char *error_diag) {
    const size_t response_len =
        zf_u2f_adapter_handle_msg(app, state->session_id, app->transport_arena, u2f_request_len,
                                  app->transport_arena, ZF_TRANSPORT_ARENA_SIZE);
    UNUSED(success_diag);
    UNUSED(error_diag);
    ZF_NFC_DIAG_EVENT(response_len >= 2U || !error_diag ? success_diag : error_diag);
    return response_len;
}

static bool zf_transport_nfc_send_encoded_u2f_immediate(ZerofidoApp *app,
                                                        ZfNfcTransportState *state,
                                                        const ZfNfcApdu *apdu,
                                                        size_t u2f_request_len,
                                                        const char *success_diag,
                                                        const char *error_diag,
                                                        bool auto_accept,
                                                        bool caller_holds_ui_mutex) {
    bool old_auto_accept = false;
    size_t response_len = 0U;

    if (auto_accept) {
        if (!caller_holds_ui_mutex) {
            furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        }
        old_auto_accept = app->transport_auto_accept_transaction;
        app->transport_auto_accept_transaction = true;
        furi_mutex_release(app->ui_mutex);
    }

    response_len = zf_transport_nfc_run_u2f_adapter(app, state, u2f_request_len, success_diag,
                                                    error_diag);

    if (auto_accept) {
        zerofido_ui_refresh_status(app);
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        app->transport_auto_accept_transaction = old_auto_accept;
        if (!caller_holds_ui_mutex) {
            furi_mutex_release(app->ui_mutex);
        }
    }

    return zf_transport_nfc_send_u2f_response(app, state, apdu, app->transport_arena,
                                              response_len);
}

static bool zf_transport_nfc_send_u2f_immediate(ZerofidoApp *app, ZfNfcTransportState *state,
                                                const ZfNfcApdu *apdu, size_t u2f_request_len,
                                                bool caller_holds_ui_mutex) {
    if (!app || !state || !apdu || u2f_request_len == 0U) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
    }

    return zf_transport_nfc_send_encoded_u2f_immediate(
        app, state, apdu, u2f_request_len, "U2F immediate", "U2F error", true,
        caller_holds_ui_mutex);
}

static bool zf_transport_nfc_send_u2f_version_immediate(ZerofidoApp *app,
                                                        ZfNfcTransportState *state,
                                                        const ZfNfcApdu *apdu, bool *handled) {
    size_t u2f_request_len = 0;

    if (handled) {
        *handled = false;
    }
    if (!app || !state || !apdu || !handled) {
        return false;
    }
    if (apdu->cla != 0x00U || apdu->ins != U2F_CMD_VERSION || apdu->chained) {
        return false;
    }

    *handled = true;
    if (zf_transport_nfc_exchange_busy(state)) {
        return zf_transport_nfc_send_busy_status(state);
    }

    const bool fallback = !state->applet_selected;
    if (fallback) {
        state->applet_selected = true;
        state->field_active = true;
        state->iso4_active = true;
        state->session_id = zf_transport_nfc_next_session_id(state->session_id);
        state->canceled_session_id = 0;
        zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletSelected);
        zf_transport_nfc_reset_exchange_locked(state);
    }
    u2f_request_len =
        zf_transport_nfc_encode_u2f_request(apdu, app->transport_arena, ZF_MAX_MSG_SIZE);
    if (u2f_request_len == 0U) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
    }

    return zf_transport_nfc_send_encoded_u2f_immediate(
        app, state, apdu, u2f_request_len, fallback ? "U2F VERSION fallback" : "U2F VERSION",
        fallback ? "U2F fallback error" : "U2F error", false, false);
}

static bool zf_transport_nfc_send_u2f_immediate_without_presence(ZerofidoApp *app,
                                                                 ZfNfcTransportState *state,
                                                                 const ZfNfcApdu *apdu,
                                                                 bool *handled) {
    const char *operation = NULL;
    size_t u2f_request_len = 0U;
    size_t response_len = 0U;

    if (handled) {
        *handled = false;
    }
    if (!app || !state || !apdu || !handled || apdu->cla != 0x00U || apdu->chained) {
        return false;
    }

    u2f_request_len =
        zf_transport_nfc_encode_u2f_request(apdu, app->transport_arena, ZF_MAX_MSG_SIZE);
    if (u2f_request_len == 0U) {
        *handled = true;
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
    }

    response_len =
        u2f_validate_request_into_response(app->transport_arena, (uint16_t)u2f_request_len,
                                           app->transport_arena, ZF_TRANSPORT_ARENA_SIZE);
    if (response_len != 0U) {
        *handled = true;
        ZF_NFC_DIAG_EVENT("U2F invalid");
        return zf_transport_nfc_send_u2f_response(app, state, apdu, app->transport_arena,
                                                  response_len);
    }

    if (u2f_request_needs_user_presence(app->transport_arena, (uint16_t)u2f_request_len,
                                        &operation)) {
        return false;
    }

    *handled = true;
    return zf_transport_nfc_send_encoded_u2f_immediate(
        app, state, apdu, u2f_request_len,
        apdu->ins == U2F_CMD_AUTHENTICATE
            ? "U2F check-only"
            : (apdu->ins == U2F_CMD_VERSION ? "U2F VERSION" : "U2F immediate"),
        NULL, false, false);
}

static bool zf_transport_nfc_finish_ctap2_msg(ZerofidoApp *app, ZfNfcTransportState *state,
                                              bool caller_holds_ui_mutex,
                                              bool request_was_extended_apdu) {
    const bool ctap_get_response_supported = state->ctap_get_response_supported;

    state->command_chain_active = false;
    if (!ctap_get_response_supported) {
        ZF_NFC_DIAG_EVENT("CTAP2 direct noGR");
        return zf_transport_nfc_send_ctap2_direct(app, state, app->transport_arena,
                                                  state->request_len, caller_holds_ui_mutex,
                                                  request_was_extended_apdu);
    }
    if (!zf_transport_nfc_queue_request_locked(app, state, ZfNfcRequestKindCtap2,
                                               app->transport_arena, state->request_len)) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_CONDITIONS_NOT_SATISFIED);
    }
    state->ctap_get_response_supported = ctap_get_response_supported;
    if (!zf_transport_nfc_wake_request_worker(app, state, caller_holds_ui_mutex)) {
        zf_transport_nfc_reset_exchange_locked(state);
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_CONDITIONS_NOT_SATISFIED);
    }
    ZF_NFC_DIAG_EVENT("CTAP2 queued");
    return zf_transport_nfc_send_busy_status(state);
}

/*
 * NFC APDU route table:
 * - SELECT FIDO applet selects the FIDO surface.
 * - SELECT NDEF is rejected so the tag surface stays FIDO-only.
 * - CTAP control END clears selected applet/session state.
 * - CTAP2 MSG runs direct for iOS no-GR readers and queues readers that support GET_RESPONSE.
 * - ISO GET RESPONSE drains pending APDU/chained response data.
 * - CLA=00 U2F APDUs route through the legacy U2F adapter.
 */
static bool zf_transport_nfc_handle_apdu_internal(ZerofidoApp *app, ZfNfcTransportState *state,
                                                  const uint8_t *apdu_bytes, size_t apdu_len,
                                                  bool caller_holds_ui_mutex) {
    ZfNfcApdu apdu;
    ZfResolvedCapabilities capabilities;
    uint8_t raw_cla = 0U;

    if (!app || !state || !apdu_bytes) {
        return false;
    }

    if (!zf_app_transport_arena_acquire(app)) {
        return false;
    }
    zf_transport_nfc_attach_arena(state, app->transport_arena,
                                  zf_app_transport_arena_capacity(app));
    zf_runtime_get_effective_capabilities(app, &capabilities);

    if (!zf_transport_nfc_parse_apdu(apdu_bytes, apdu_len, &apdu)) {
        ZF_NFC_DIAG_EVENT("NFC APDU parse failed");
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
    }

    raw_cla = apdu_bytes[0];
    if (apdu.cla == 0x00U && apdu.ins == 0xA4U && apdu.p1 == 0x04U &&
        (apdu.p2 == 0x00U || apdu.p2 == 0x0CU) && apdu.data_len == ZF_NFC_NDEF_AID_LEN &&
        memcmp(apdu.data, zf_transport_nfc_ndef_aid, ZF_NFC_NDEF_AID_LEN) == 0) {
        state->applet_selected = false;
        zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
        ZF_NFC_DIAG_EVENT("NDEF reject");
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_FILE_NOT_FOUND);
    }

    if (apdu.cla == 0x00U && apdu.ins == 0xA4U) {
        const bool valid_select = zf_transport_nfc_is_fido_select_apdu(&apdu);

#if ZF_RELEASE_DIAGNOSTICS
        if (!valid_select) {
            char text[sizeof(app->status_text)];

            snprintf(text, sizeof(text), "SELECT %02X %02X lc=%u", apdu.p1, apdu.p2,
                     (unsigned)apdu.data_len);
            ZF_NFC_DIAG_EVENT(text);
        } else {
            ZF_NFC_DIAG_EVENT("FIDO SELECT");
        }
#else
        (void)valid_select;
#endif
        const bool handled = zf_transport_nfc_handle_select(state, &apdu, &capabilities);
        return handled;
    }

    if (!state->applet_selected && capabilities.u2f_enabled) {
        bool u2f_version_handled = false;
        const bool u2f_version_result =
            zf_transport_nfc_send_u2f_version_immediate(app, state, &apdu, &u2f_version_handled);
        if (u2f_version_handled) {
            return u2f_version_result;
        }
    }

    if (raw_cla == 0x90U && apdu.ins == 0x60U && apdu.p1 == 0x00U && apdu.p2 == 0x00U) {
        zf_transport_nfc_note_ui_stage_locked(
            state, state->applet_selected ? ZfNfcUiStageAppletSelected : ZfNfcUiStageAppletWaiting);
        return zf_transport_nfc_send_desfire_version_terminal(app, state);
    }

    if (!state->applet_selected) {
#if ZF_RELEASE_DIAGNOSTICS
        char text[sizeof(app->status_text)];
#endif

        if (raw_cla == 0x90U && apdu.ins == 0xAFU) {
            state->applet_selected = false;
            zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
            return zf_transport_nfc_send_desfire_version_part(app, state,
                                                              state->desfire_probe_frame >= 2U);
        }

        if (raw_cla == 0x80U && apdu.ins == 0x60U && apdu.p1 == 0x00U && apdu.p2 == 0x00U) {
            ZF_NFC_DIAG_EVENT("APDU 8060 reject");
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INS_NOT_SUPPORTED);
        }
#if ZF_RELEASE_DIAGNOSTICS
        snprintf(text, sizeof(text), "APDU %02X %02X %02X %02X", apdu.cla, apdu.ins, apdu.p1,
                 apdu.p2);
        ZF_NFC_DIAG_EVENT(text);
#endif
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_CONDITIONS_NOT_SATISFIED);
    }

    if (apdu.cla == 0x80U && apdu.ins == ZF_NFC_INS_CTAP_CONTROL && apdu.p1 == 0x01U &&
        apdu.p2 == 0x00U) {
        state->applet_selected = false;
        zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
        zf_transport_nfc_cancel_current_request_locked(state);
        zf_transport_nfc_reset_exchange_locked(state);
        zf_transport_nfc_clear_last_iso_response(state);
        zf_transport_nfc_clear_tx_chain(state);
        zerofido_ui_cancel_pending_interaction_locked(app);
        ZF_NFC_DIAG_EVENT("CTAP control END");
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_SUCCESS);
    }

    if (apdu.cla == 0x80U && apdu.ins == ZF_NFC_INS_CTAP_MSG && !apdu.chained &&
        apdu.data_len == 1U && apdu.data && apdu.data[0] == ZfCtapeCmdGetInfo) {
        if (!zf_transport_nfc_ctap_msg_p1p2_valid(&apdu)) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_P1P2);
        }
        ZF_NFC_DIAG_EVENT("CTAP2 getInfo");
        if (!capabilities.fido2_enabled) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INS_NOT_SUPPORTED);
        }
        if (zf_transport_nfc_exchange_busy(state)) {
            return zf_transport_nfc_send_busy_status(state);
        }
        return zf_transport_nfc_send_get_info_response(app, state, &capabilities,
                                                       caller_holds_ui_mutex);
    }

    if ((apdu.cla == 0x80U && apdu.ins == ZF_NFC_INS_CTAP_GET_RESPONSE) ||
        ((apdu.cla == 0x80U || apdu.cla == 0x00U) && apdu.ins == ZF_NFC_INS_ISO_GET_RESPONSE)) {
        if (apdu.ins == ZF_NFC_INS_CTAP_GET_RESPONSE && (apdu.p1 != 0x00U || apdu.p2 != 0x00U)) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_P1P2);
        }
        return zf_transport_nfc_send_get_response(app, state, &apdu);
    }

    if (apdu.chained && apdu.cla == 0x80U && apdu.ins == ZF_NFC_INS_CTAP_MSG) {
        if (!zf_transport_nfc_ctap_msg_p1p2_valid(&apdu)) {
            zf_transport_nfc_reset_exchange_locked(state);
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_P1P2);
        }
        if (zf_transport_nfc_exchange_busy(state)) {
            return zf_transport_nfc_send_busy_status(state);
        }
        if (!zf_transport_nfc_append_ctap_chained_fragment(app, state, &apdu)) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
        }

        state->command_chain_active = true;
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_SUCCESS);
    }

    if (apdu.cla == 0x80U && apdu.ins == ZF_NFC_INS_CTAP_MSG) {
        if (!zf_transport_nfc_ctap_msg_p1p2_valid(&apdu)) {
            zf_transport_nfc_reset_exchange_locked(state);
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_P1P2);
        }
        if (!capabilities.fido2_enabled) {
            ZF_NFC_DIAG_EVENT("CTAP2 disabled");
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INS_NOT_SUPPORTED);
        }
        if (!state->ctap_get_response_supported &&
            (state->response_ready || state->response_is_error)) {
            ZF_NFC_DIAG_EVENT("CTAP2 original response");
            return zf_transport_nfc_send_get_response(app, state, &apdu);
        }
        if (zf_transport_nfc_exchange_busy(state)) {
            return zf_transport_nfc_send_busy_status(state);
        }
        if (state->command_chain_active) {
            if (!zf_transport_nfc_append_ctap_chained_fragment(app, state, &apdu)) {
                return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
            }
        } else {
            if (!zf_transport_nfc_begin_ctap_msg(app, state, &apdu)) {
                return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
            }
        }

        return zf_transport_nfc_finish_ctap2_msg(app, state, caller_holds_ui_mutex, apdu.extended);
    }

    if (apdu.cla == 0x00U) {
        size_t u2f_request_len = 0;
        bool u2f_immediate_handled = false;
        bool u2f_immediate_result = false;

        if (!capabilities.u2f_enabled) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INS_NOT_SUPPORTED);
        }
        if (apdu.chained) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INS_NOT_SUPPORTED);
        }
        if (zf_transport_nfc_exchange_busy(state)) {
            return zf_transport_nfc_send_busy_status(state);
        }
        u2f_immediate_result = zf_transport_nfc_send_u2f_immediate_without_presence(
            app, state, &apdu, &u2f_immediate_handled);
        if (u2f_immediate_handled) {
            return u2f_immediate_result;
        }
        u2f_request_len =
            zf_transport_nfc_encode_u2f_request(&apdu, app->transport_arena, ZF_MAX_MSG_SIZE);
        if (u2f_request_len == 0) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
        }
        return zf_transport_nfc_send_u2f_immediate(app, state, &apdu, u2f_request_len,
                                                   caller_holds_ui_mutex);
    }

    return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INS_NOT_SUPPORTED);
}

bool zf_transport_nfc_handle_apdu(ZerofidoApp *app, ZfNfcTransportState *state,
                                  const uint8_t *apdu_bytes, size_t apdu_len) {
    return zf_transport_nfc_handle_apdu_internal(app, state, apdu_bytes, apdu_len, false);
}

bool zf_transport_nfc_handle_apdu_locked(ZerofidoApp *app, ZfNfcTransportState *state,
                                         const uint8_t *apdu_bytes, size_t apdu_len) {
    return zf_transport_nfc_handle_apdu_internal(app, state, apdu_bytes, apdu_len, true);
}

#endif
