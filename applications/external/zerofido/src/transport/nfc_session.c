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

#include "nfc_session.h"

#include <furi.h>
#include <string.h>

#include "../u2f/adapter.h"
#include "../zerofido_crypto.h"
#include "../zerofido_notify.h"
#include "../zerofido_telemetry.h"
#include "../zerofido_ui.h"
#include "../zerofido_ui_i.h"
#include "nfc_iso_dep.h"

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS && !defined(ZF_HOST_TEST)
#define ZF_NFC_MEM_DIAG(event) zf_telemetry_log(event)
#else
#define ZF_NFC_MEM_DIAG(event)                                                                     \
    do {                                                                                           \
    } while (false)
#endif

void zf_transport_nfc_attach_arena(ZfNfcTransportState *state, uint8_t *arena,
                                   size_t arena_capacity) {
    if (!state) {
        return;
    }

    state->arena = arena;
    state->arena_capacity = arena_capacity;
}

uint8_t *zf_transport_nfc_arena(const ZfNfcTransportState *state) {
    return state ? state->arena : NULL;
}

size_t zf_transport_nfc_arena_capacity(const ZfNfcTransportState *state) {
    return state ? state->arena_capacity : 0U;
}

uint32_t zf_transport_nfc_next_session_id(ZfTransportSessionId current) {
    current++;
    return current == 0 ? 1U : current;
}

static uint32_t zf_transport_nfc_next_generation(uint32_t current) {
    current++;
    return current == 0U ? 1U : current;
}

void zf_transport_nfc_note_ui_stage_locked(ZfNfcTransportState *state, ZfNfcUiStage stage) {
    if (!state) {
        return;
    }

    state->last_visible_stage = (uint8_t)stage;
    state->last_visible_stage_tick = furi_get_tick();
}

bool zf_transport_nfc_request_worker_active(const ZfNfcTransportState *state) {
    return state && (state->processing || state->request_pending);
}

/* Resets a single NFC exchange and scrubs any request/response bytes in the arena. */
static void zf_transport_nfc_reset_exchange_internal_locked(ZfNfcTransportState *state) {
    size_t pending_len = 0U;

    if (!state) {
        return;
    }

    pending_len = state->request_len;
    if (pending_len > ZF_MAX_MSG_SIZE) {
        pending_len = ZF_MAX_MSG_SIZE;
    }
    if (state->request_pending && pending_len > 0U) {
        uint8_t *arena = zf_transport_nfc_arena(state);
        size_t arena_capacity = zf_transport_nfc_arena_capacity(state);
        if (arena && pending_len <= arena_capacity) {
            zf_crypto_secure_zero(arena, pending_len);
        }
    }
    state->request_pending = false;
    state->processing = false;
    state->processing_cancel_requested = false;
    state->response_ready = false;
    state->response_is_u2f = false;
    state->response_is_error = false;
    state->command_chain_active = false;
    state->ctap_get_response_supported = false;
    zf_transport_nfc_clear_tx_chain(state);
    state->request_len = 0;
    state->response_len = 0;
    state->response_offset = 0;
    state->rx_chain_last_valid = false;
    state->rx_complete_last_valid = false;
    state->rx_complete_last_cid_present = false;
    state->rx_complete_last_response_preserves_replay = false;
    zf_crypto_secure_zero(state->rx_complete_last_payload,
                          sizeof(state->rx_complete_last_payload));
    zf_crypto_secure_zero(state->rx_complete_last_response,
                          sizeof(state->rx_complete_last_response));
    state->rx_chain_last_pcb = 0U;
    state->rx_complete_last_pcb = 0U;
    state->rx_complete_last_cid = 0U;
    state->rx_complete_last_iso_pcb = 0U;
    state->rx_chain_duplicate_count = 0U;
    state->rx_chain_last_offset = 0U;
    state->rx_chain_last_len = 0U;
    state->rx_complete_last_len = 0U;
    state->rx_complete_last_response_len = 0U;
    state->error_status_word = 0;
    state->pending_status = ZF_NFC_STATUS_PROCESSING;
    state->request_kind = ZfNfcRequestKindNone;
    state->processing_session_id = 0;
    state->processing_generation = zf_transport_nfc_next_generation(state->processing_generation);
    if (state->arena && state->arena_capacity > 0U) {
        zf_crypto_secure_zero(state->arena, state->arena_capacity);
    }
}

void zf_transport_nfc_reset_exchange_locked(ZfNfcTransportState *state) {
    zf_transport_nfc_reset_exchange_internal_locked(state);
}

static void zf_transport_nfc_reset_exchange_preserving_replay_locked(ZfNfcTransportState *state) {
    zf_transport_nfc_reset_exchange_internal_locked(state);
}

void zf_transport_nfc_cancel_current_request_locked(ZfNfcTransportState *state) {
    if (!state) {
        return;
    }

    if (state->processing_session_id != 0) {
        state->canceled_session_id = state->processing_session_id;
    }
    state->processing_cancel_requested = true;
}

void zf_transport_nfc_teardown_rf_activation_locked(ZfNfcTransportState *state) {
    if (!state) {
        return;
    }

    state->field_active = false;
    state->iso4_active = false;
    state->applet_selected = false;
    state->desfire_probe_frame = 0U;
    state->iso4_listener = NULL;
    state->iso_cid_present = false;
    state->iso_cid = 0U;
    zf_transport_nfc_cancel_current_request_locked(state);
    if (state->iso4_tx_chain_completed) {
        state->post_success_cooldown_active = true;
        state->post_success_cooldown_until_tick = furi_get_tick() + ZF_NFC_POST_SUCCESS_COOLDOWN_MS;
        state->post_success_probe_sleep_active = true;
        state->iso4_tx_chain_completed = false;
    }
    zf_transport_nfc_reset_exchange_locked(state);
    zf_transport_nfc_clear_last_iso_response(state);
    zf_transport_nfc_clear_tx_chain(state);
    state->iso_pcb = ZF_NFC_PCB_BLOCK;
}

/* Field loss cancels pending UI, resets ISO state, and clears replay buffers. */
void zf_transport_nfc_on_disconnect(ZerofidoApp *app) {
    bool canceled = false;
    ZfNfcTransportState *state = NULL;

    if (!app) {
        return;
    }

    ZF_NFC_MEM_DIAG("nfc field disconnect before");
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    state = zf_app_nfc_transport_state(app);
    if (!state) {
        furi_mutex_release(app->ui_mutex);
        return;
    }
    zf_transport_nfc_teardown_rf_activation_locked(state);
    furi_mutex_release(app->ui_mutex);

    canceled = zerofido_ui_cancel_pending_interaction(app);
    zf_u2f_adapter_set_connected(app, false);
    zerofido_ui_set_transport_connected(app, false);
    zerofido_notify_reset(app);
    zerofido_ui_refresh_status_line(app);
    if (canceled) {
        zerofido_ui_dispatch_custom_event(app, ZfEventHideApproval);
    }
    ZF_NFC_MEM_DIAG("nfc field disconnect after");
}

/* Returns the one-byte NFC status-update value exposed while CTAP waits for touch. */
uint8_t zf_transport_nfc_current_status(const ZerofidoApp *app) {
    uint8_t status = ZF_NFC_STATUS_PROCESSING;

    if (!app) {
        return status;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (app->approval.state == ZfApprovalPending) {
        status = ZF_NFC_STATUS_UPNEEDED;
    }
    furi_mutex_release(app->ui_mutex);
    return status;
}

/*
 * Implements ISO7816 GET RESPONSE paging for CTAP2 and U2F. U2F responses carry
 * their final APDU status word inside the response buffer; CTAP2 uses SW_SUCCESS.
 */
bool zf_transport_nfc_send_get_response(const ZerofidoApp *app, ZfNfcTransportState *state,
                                        const ZfNfcApdu *apdu) {
    size_t le = zf_transport_nfc_normalize_le(apdu);
    size_t remaining = 0;
    size_t chunk_len = 0;
    uint8_t *arena = zf_transport_nfc_arena(state);
    size_t arena_capacity = zf_transport_nfc_arena_capacity(state);

    if (le > ZF_NFC_GET_RESPONSE_CHUNK_SIZE) {
        le = ZF_NFC_GET_RESPONSE_CHUNK_SIZE;
    }

    if (state->processing && !state->response_ready) {
        if (!state->ctap_get_response_supported) {
            return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_CONDITIONS_NOT_SATISFIED);
        }
        const uint8_t status = zf_transport_nfc_current_status(app);
        return zf_transport_nfc_send_apdu_payload(state, &status, 1, ZF_NFC_SW_STATUS_UPDATE);
    }

    if (state->response_is_error) {
        const uint16_t error_status =
            state->error_status_word ? state->error_status_word : ZF_NFC_SW_INTERNAL_ERROR;
        zf_transport_nfc_reset_exchange_locked(state);
        return zf_transport_nfc_send_status_word(state, error_status);
    }

    if (!state->response_ready) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_CONDITIONS_NOT_SATISFIED);
    }

    if (!arena || state->response_len > arena_capacity ||
        state->response_offset > state->response_len ||
        (state->response_is_u2f && state->response_len < 2U)) {
        zf_transport_nfc_reset_exchange_locked(state);
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
    }

    remaining = state->response_len - state->response_offset;
    if (state->response_is_u2f) {
        size_t payload_remaining = 0;

        if (remaining <= 2U) {
            const uint16_t status_word =
                ((uint16_t)arena[state->response_len - 2U] << 8) | arena[state->response_len - 1U];
            zf_transport_nfc_reset_exchange_locked(state);
            return zf_transport_nfc_send_status_word(state, status_word);
        }

        payload_remaining = remaining - 2U;
        if (payload_remaining <= le) {
            chunk_len = payload_remaining;
            const uint16_t status_word =
                ((uint16_t)arena[state->response_len - 2U] << 8) | arena[state->response_len - 1U];
            const bool ok = zf_transport_nfc_send_apdu_payload(
                state, &arena[state->response_offset], chunk_len, status_word);
            zf_transport_nfc_reset_exchange_preserving_replay_locked(state);
            return ok;
        } else {
            chunk_len = le;
        }
    } else if (remaining <= le) {
        const bool ok = zf_transport_nfc_send_apdu_payload(state, &arena[state->response_offset],
                                                           remaining, ZF_NFC_SW_SUCCESS);
        zf_transport_nfc_reset_exchange_preserving_replay_locked(state);
        return ok;
    } else {
        chunk_len = le;
    }

    state->response_offset += chunk_len;
    remaining = state->response_len - state->response_offset;
    return zf_transport_nfc_send_apdu_payload(state, &arena[state->response_offset - chunk_len],
                                              chunk_len,
                                              zf_transport_nfc_status_update_sw(remaining));
}

bool zf_transport_nfc_send_deferred_response_or_wtx(const ZerofidoApp *app,
                                                    ZfNfcTransportState *state) {
    uint8_t *arena = zf_transport_nfc_arena(state);
    size_t arena_capacity = zf_transport_nfc_arena_capacity(state);

    (void)app;
    if (!state) {
        return false;
    }

    if (state->processing && !state->response_ready && !state->response_is_error) {
        return zf_transport_nfc_send_apdu_payload(
            state, (const uint8_t[]){ZF_NFC_STATUS_PROCESSING}, 1U, ZF_NFC_SW_STATUS_UPDATE);
    }

    if (state->response_is_error) {
        const uint16_t error_status =
            state->error_status_word ? state->error_status_word : ZF_NFC_SW_INTERNAL_ERROR;
        const bool sent = zf_transport_nfc_send_status_word(state, error_status);
        zf_transport_nfc_reset_exchange_locked(state);
        return sent;
    }

    if (!state->response_ready) {
        return zf_transport_nfc_send_apdu_payload(
            state, (const uint8_t[]){ZF_NFC_STATUS_PROCESSING}, 1U, ZF_NFC_SW_STATUS_UPDATE);
    }

    if (!arena || state->response_len > arena_capacity) {
        const bool sent = zf_transport_nfc_send_status_word(state, ZF_NFC_SW_INTERNAL_ERROR);
        zf_transport_nfc_reset_exchange_locked(state);
        return sent;
    }

    if (state->response_len + 2U <= ZF_NFC_MAX_TX_FRAME_INF_SIZE) {
        const bool sent = zf_transport_nfc_send_apdu_payload(state, arena, state->response_len,
                                                             ZF_NFC_SW_SUCCESS);
        zf_transport_nfc_reset_exchange_locked(state);
        return sent;
    }

    return zf_transport_nfc_begin_chained_apdu_payload(state, arena, state->response_len,
                                                       ZF_NFC_SW_SUCCESS);
}

/* Queues one APDU payload for asynchronous protocol dispatch by the NFC worker. */
bool zf_transport_nfc_queue_request_locked(ZerofidoApp *app, ZfNfcTransportState *state,
                                           ZfNfcRequestKind request_kind, const uint8_t *request,
                                           size_t request_len) {
    uint8_t *arena = NULL;

    if (app && zf_app_transport_arena_acquire(app)) {
        arena = app->transport_arena;
    }

    if (state->stopping || request_len == 0 || !arena || request_len > ZF_MAX_MSG_SIZE ||
        state->processing || state->request_pending ||
        zf_transport_nfc_request_worker_active(state)) {
        state->response_is_error = true;
        state->error_status_word = ZF_NFC_SW_CONDITIONS_NOT_SATISFIED;
        return false;
    }

    zf_transport_nfc_attach_arena(state, arena, zf_app_transport_arena_capacity(app));
    if (request != arena) {
        memmove(arena, request, request_len);
    }
    state->request_len = request_len;
    state->request_kind = request_kind;
    state->request_pending = true;
    state->processing = true;
    state->processing_cancel_requested = false;
    state->canceled_session_id = 0;
    state->processing_session_id = state->session_id;
    state->request_generation = zf_transport_nfc_next_generation(state->request_generation);
    state->processing_generation = state->request_generation;
    state->response_ready = false;
    state->response_is_error = false;
    state->response_is_u2f = false;
    state->response_len = 0U;
    state->response_offset = 0;
    state->pending_status = ZF_NFC_STATUS_PROCESSING;

    return true;
}

/* SELECT accepts the FIDO AID, starts a fresh logical session, and chooses ATS metadata. */
bool zf_transport_nfc_handle_select(ZfNfcTransportState *state, const ZfNfcApdu *apdu,
                                    const ZfResolvedCapabilities *capabilities) {
    const uint8_t *select_response = zf_transport_nfc_select_response;
    size_t select_response_len = sizeof(zf_transport_nfc_select_response);

    if (!zf_transport_nfc_is_fido_select_apdu(apdu)) {
        return zf_transport_nfc_send_status_word(state, ZF_NFC_SW_FILE_NOT_FOUND);
    }

    if (capabilities && capabilities->fido2_enabled && !capabilities->u2f_enabled) {
        select_response = zf_transport_nfc_fido2_select_response;
        select_response_len = sizeof(zf_transport_nfc_fido2_select_response);
    }

    state->applet_selected = true;
    state->post_success_cooldown_active = false;
    state->post_success_probe_sleep_active = false;
    state->post_success_cooldown_until_tick = 0U;
    state->field_active = true;
    state->iso4_active = true;
    state->session_id = zf_transport_nfc_next_session_id(state->session_id);
    state->canceled_session_id = 0;
    zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletSelected);
    zf_transport_nfc_reset_exchange_locked(state);
    zf_transport_nfc_clear_last_iso_response(state);
    zf_transport_nfc_clear_tx_chain(state);
    return zf_transport_nfc_send_apdu_payload(state, select_response, select_response_len,
                                              ZF_NFC_SW_SUCCESS);
}

/* Publishes worker output only if it still belongs to the active NFC request generation. */
void zf_transport_nfc_store_response(ZerofidoApp *app, ZfNfcTransportState *state,
                                     ZfTransportSessionId session_id, uint32_t request_generation,
                                     const uint8_t *response, size_t response_len,
                                     bool response_is_u2f, bool response_is_error,
                                     uint16_t error_status_word) {
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (state->processing_session_id != session_id ||
        state->processing_generation != request_generation || state->processing_cancel_requested) {
        furi_mutex_release(app->ui_mutex);
        return;
    }

    {
        uint8_t *arena = NULL;
        size_t arena_capacity = 0U;

        if (app && zf_app_transport_arena_acquire(app)) {
            zf_transport_nfc_attach_arena(state, app->transport_arena,
                                          zf_app_transport_arena_capacity(app));
        }
        arena = zf_transport_nfc_arena(state);
        arena_capacity = zf_transport_nfc_arena_capacity(state);

        if (!arena || response_len > arena_capacity ||
            (!response_is_error && response_len > 0U && !response) ||
            (response_is_u2f && !response_is_error && response_len < 2U)) {
            response_len = 0;
            response_is_u2f = false;
            response_is_error = true;
            error_status_word = ZF_NFC_SW_INTERNAL_ERROR;
            if (arena && arena_capacity > 0U) {
                zf_crypto_secure_zero(arena, arena_capacity);
            }
        } else if (response_len > 0 && response && response != arena) {
            zf_crypto_secure_zero(arena, arena_capacity);
            memcpy(arena, response, response_len);
        }
        state->response_len = response_len;
        state->response_offset = 0;
        state->response_ready = !response_is_error;
        state->response_is_u2f = response_is_u2f;
        state->response_is_error = response_is_error;
        state->error_status_word = error_status_word;
    }
    state->processing = false;
    state->request_pending = false;
    state->processing_session_id = 0;
    furi_mutex_release(app->ui_mutex);
}

#endif
