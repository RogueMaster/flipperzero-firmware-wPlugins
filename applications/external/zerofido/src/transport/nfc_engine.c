/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#ifndef ZF_USB_ONLY

#include "nfc_engine.h"

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <lib/toolbox/simple_array.h>
#include <stdio.h>
#include <string.h>
#include <toolbox/bit_buffer.h>

#include "../u2f/adapter.h"
#include "../zerofido_app_i.h"
#include "../zerofido_ui.h"
#include "nfc_dispatch.h"
#include "nfc_iso_dep.h"
#include "nfc_protocol.h"
#include "nfc_session.h"
#include "nfc_trace.h"

#define ZF_NFC_DESFIRE_CMD_GET_VERSION 0x60U
#define ZF_NFC_ATS_T0_TA1 0x10U
#define ZF_NFC_ATS_T0_TB1 0x20U
#define ZF_NFC_ATS_T0_TC1 0x40U
#define ZF_NFC_ATS_TC1_CID 0x02U
#define ZF_NFC_RATS_CMD 0xE0U
#define ZF_NFC_REQA_CMD 0x26U
#define ZF_NFC_WUPA_CMD 0x52U
#define ZF_NFC_DESFIRE_CMD_ADDITIONAL_FRAME 0xAFU
#define ZF_NFC_DESFIRE_STATUS_MORE 0xAFU
#define ZF_NFC_DESFIRE_STATUS_OK 0x00U
#define ZF_NFC_PPS_START 0xD0U
#define ZF_NFC_PPS_START_MASK 0xF0U
#define ZF_NFC_PPS_RECOVERY_REPLAY_MIN_LEN ZF_NFC_MAX_TX_FRAME_INF_SIZE
#define ZF_NFC_RX_CHAIN_DUPLICATE_LIMIT 8U

#if ZF_RELEASE_DIAGNOSTICS
static bool zf_transport_nfc_format_frame_status(char *text, size_t text_len, const char *prefix,
                                                 uint8_t pcb, const uint8_t *payload,
                                                 size_t payload_len) {
    char suffix[32] = {0};
    size_t offset = 0;

    if (!text || text_len == 0U || !prefix) {
        return false;
    }

    for (size_t i = 0; payload && i < payload_len && i < 3U; ++i) {
        int written = snprintf(&suffix[offset], sizeof(suffix) - offset, " %02X", payload[i]);
        if (written <= 0 || (size_t)written >= (sizeof(suffix) - offset)) {
            break;
        }
        offset += (size_t)written;
    }

    snprintf(text, text_len, "%s %02X len=%u%s", prefix, pcb, (unsigned)payload_len, suffix);
    return true;
}

static void zf_transport_nfc_set_frame_status(const char *prefix, uint8_t pcb,
                                              const uint8_t *payload, size_t payload_len) {
    char text[64];

    if (!zf_transport_nfc_format_frame_status(text, sizeof(text), prefix, pcb, payload,
                                              payload_len)) {
        return;
    }

    zf_transport_nfc_trace_event(text);
}

#else
static inline void zf_transport_nfc_set_frame_status(const char *prefix, uint8_t pcb,
                                                     const uint8_t *payload, size_t payload_len) {
    (void)prefix;
    (void)pcb;
    (void)payload;
    (void)payload_len;
}
#endif

static bool zf_transport_nfc_is_bare_apdu(const uint8_t *frame, size_t frame_len) {
    ZfNfcApdu apdu;

    if (!frame || frame_len < 4U) {
        return false;
    }

    if (frame[0] != 0x00U && frame[0] != 0x80U && frame[0] != 0x90U) {
        return false;
    }

    return zf_transport_nfc_parse_apdu(frame, frame_len, &apdu);
}

static bool zf_transport_nfc_is_native_desfire_get_version_payload(const uint8_t *payload,
                                                                   size_t payload_len) {
    return payload && payload_len == 1U && payload[0] == ZF_NFC_DESFIRE_CMD_GET_VERSION;
}

static bool zf_transport_nfc_is_native_desfire_additional_frame(const uint8_t *payload,
                                                                size_t payload_len) {
    return payload && payload_len == 1U && payload[0] == ZF_NFC_DESFIRE_CMD_ADDITIONAL_FRAME;
}

static bool zf_transport_nfc_is_i_block(uint8_t pcb) {
    return (pcb & 0xC2U) == 0x02U;
}

static bool zf_transport_nfc_is_r_block(uint8_t pcb) {
    return (pcb & 0xE2U) == 0xA2U;
}

static bool zf_transport_nfc_is_r_nack_block(uint8_t pcb) {
    return zf_transport_nfc_is_r_block(pcb) && (pcb & 0x10U) != 0U;
}

static bool zf_transport_nfc_is_one_byte_recovery_r_nak(uint8_t pcb) {
    return pcb == 0xB2U || pcb == 0xB3U;
}

static bool zf_transport_nfc_is_s_block(uint8_t pcb) {
    return (pcb & 0xC2U) == 0xC2U;
}

static bool zf_transport_nfc_is_s_wtx_block(uint8_t pcb) {
    return zf_transport_nfc_is_s_block(pcb) && (pcb & 0x30U) == 0x30U;
}

static bool zf_transport_nfc_is_s_deselect_block(uint8_t pcb) {
    return zf_transport_nfc_is_s_block(pcb) && !zf_transport_nfc_is_s_wtx_block(pcb);
}

static bool zf_transport_nfc_is_pps_frame(const uint8_t *frame, size_t frame_len) {
    if (!frame || frame_len < 2U) {
        return false;
    }

    return (frame[0] & ZF_NFC_PPS_START_MASK) == ZF_NFC_PPS_START;
}

static bool zf_transport_nfc_send_pps_ack(ZfNfcTransportState *state, uint8_t ppss) {
    return zf_transport_nfc_send_frame(state, &ppss, 1U);
}

static bool zf_transport_nfc_has_cached_replay(const ZfNfcTransportState *state) {
    return state && state->iso4_last_tx_valid && state->iso4_last_tx_len > 0U &&
           state->iso4_last_tx_len <= sizeof(state->iso4_tx_frame);
}

static bool zf_transport_nfc_has_large_cached_replay(const ZfNfcTransportState *state) {
    return zf_transport_nfc_has_cached_replay(state) &&
           state->iso4_last_tx_len >= ZF_NFC_PPS_RECOVERY_REPLAY_MIN_LEN;
}

static NfcCommand zf_transport_nfc_finish_unlocked(ZerofidoApp *app, bool connected,
                                                   bool refresh_status, NfcCommand command) {
    zerofido_ui_set_transport_connected(app, connected);
    zf_u2f_adapter_set_connected(app, connected);
    if (refresh_status) {
        zerofido_ui_refresh_status_line(app);
    }
    return command;
}

static void zf_transport_nfc_ack_completed_tx_chain_locked(ZfNfcTransportState *state) {
    if (!state) {
        return;
    }

    state->iso4_tx_chain_completed = false;
}

static bool zf_transport_nfc_post_success_cooldown_active_locked(ZfNfcTransportState *state) {
    if (!state || !state->post_success_cooldown_active) {
        return false;
    }

    if ((int32_t)(furi_get_tick() - state->post_success_cooldown_until_tick) >= 0) {
        state->post_success_cooldown_active = false;
        state->post_success_probe_sleep_active = false;
        state->post_success_cooldown_until_tick = 0U;
        return false;
    }

    return true;
}

static NfcCommand zf_transport_nfc_sleep_probe_locked(ZerofidoApp *app, ZfNfcTransportState *state,
                                                      const char *status) {
    if (state) {
        state->field_active = false;
        state->iso4_active = false;
        state->applet_selected = false;
        state->desfire_probe_frame = 0U;
        state->iso4_listener = NULL;
        state->iso_pcb = ZF_NFC_PCB_BLOCK;
        state->iso_cid_present = false;
        state->iso_cid = 0U;
        zf_transport_nfc_reset_exchange_locked(state);
        zf_transport_nfc_clear_last_iso_response(state);
        zf_transport_nfc_clear_tx_chain(state);
    }

    if (app) {
#if ZF_RELEASE_DIAGNOSTICS
        zf_transport_nfc_trace_event(status ? status : "NFC sleep");
#else
        (void)status;
#endif
        furi_mutex_release(app->ui_mutex);
    }
    return NfcCommandSleep;
}

static bool zf_transport_nfc_should_sleep_post_success_probe_locked(ZfNfcTransportState *state,
                                                                    const uint8_t *payload,
                                                                    size_t payload_len) {
    if (!state || (!payload && payload_len > 0U)) {
        return false;
    }

    if (zf_transport_nfc_post_success_cooldown_active_locked(state)) {
        if (zf_transport_nfc_is_native_desfire_get_version_payload(payload, payload_len) ||
            zf_transport_nfc_is_native_desfire_additional_frame(payload, payload_len)) {
            return true;
        }

        if (zf_transport_nfc_is_ndef_select_apdu(payload, payload_len)) {
            state->post_success_probe_sleep_active = false;
            return false;
        }

        return false;
    }

    return false;
}

static NfcCommand zf_transport_nfc_handle_empty_i_block_locked(ZerofidoApp *app,
                                                               ZfNfcTransportState *state,
                                                               uint8_t request_pcb,
                                                               bool refresh_status) {
    const bool replay_available = zf_transport_nfc_has_cached_replay(state);
    const bool replayed = zf_transport_nfc_replay_last_iso_response(state);
    const bool sent =
        replayed || (!replay_available && zf_transport_nfc_send_r_ack(state, request_pcb));

    zf_transport_nfc_set_frame_status(replayed ? "NFC I-empty replay" : "NFC I-empty", request_pcb,
                                      NULL, 0U);
    furi_mutex_release(app->ui_mutex);
    if (!sent) {
        zf_transport_nfc_set_frame_status("NFC I-empty fail", request_pcb, NULL, 0U);
    }
    return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
}

static NfcCommand zf_transport_nfc_handle_recovery_r_nak_locked(ZerofidoApp *app,
                                                                ZfNfcTransportState *state,
                                                                const uint8_t *frame,
                                                                size_t frame_len,
                                                                bool refresh_status) {
    const bool sent = zf_transport_nfc_replay_last_iso_response(state);

    UNUSED(frame_len);
    zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
    furi_mutex_release(app->ui_mutex);
    zf_transport_nfc_set_frame_status(sent ? "NFC R-NAK replay" : "NFC R-NAK", frame[0], NULL, 0U);
    return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
}

static void zf_transport_nfc_sync_response_pcb(ZfNfcTransportState *state, uint8_t request_pcb) {
    if (state) {
        state->iso_pcb = (uint8_t)(ZF_NFC_PCB_BLOCK | (request_pcb & 0x01U));
    }
}

static bool zf_transport_nfc_sync_i_block_header(ZfNfcTransportState *state, const uint8_t *frame,
                                                 size_t frame_len, size_t *payload_offset) {
    size_t offset = 1U;

    if (!state || !frame || !payload_offset || frame_len == 0U) {
        return false;
    }

    zf_transport_nfc_sync_response_pcb(state, frame[0]);
    state->iso4_tx_chain_completed = false;
    state->iso_cid_present = (frame[0] & ZF_NFC_PCB_CID) != 0U;
    if (state->iso_cid_present) {
        if (!state->iso14443_4a_data ||
            (state->iso14443_4a_data->ats_data.tc_1 & ZF_NFC_ATS_TC1_CID) == 0U) {
            state->iso_cid_present = false;
            state->iso_cid = 0U;
            return false;
        }
        if (frame_len < 2U) {
            return false;
        }
        state->iso_cid = frame[1];
        offset++;
    } else {
        state->iso_cid = 0U;
    }

    *payload_offset = offset;
    return frame_len >= offset;
}

static void zf_transport_nfc_remember_complete_i_block_locked(ZfNfcTransportState *state,
                                                              uint8_t request_pcb,
                                                              const uint8_t *payload,
                                                              size_t payload_len) {
    if (!state || (!payload && payload_len > 0U) ||
        payload_len > sizeof(state->rx_complete_last_payload) || state->tx_frame_len == 0U ||
        state->tx_frame_len > sizeof(state->rx_complete_last_response) ||
        !zf_transport_nfc_is_i_block(state->iso4_tx_frame[0])) {
        if (state) {
            state->rx_complete_last_valid = false;
            state->rx_complete_last_len = 0U;
            state->rx_complete_last_response_len = 0U;
            memset(state->rx_complete_last_payload, 0,
                   sizeof(state->rx_complete_last_payload));
            memset(state->rx_complete_last_response, 0,
                   sizeof(state->rx_complete_last_response));
        }
        return;
    }

    if (payload_len > 0U) {
        memcpy(state->rx_complete_last_payload, payload, payload_len);
    }
    memcpy(state->rx_complete_last_response, state->iso4_tx_frame, state->tx_frame_len);
    state->rx_complete_last_pcb = request_pcb;
    state->rx_complete_last_cid_present = state->iso_cid_present;
    state->rx_complete_last_cid = state->iso_cid;
    state->rx_complete_last_iso_pcb = state->iso_pcb;
    state->rx_complete_last_len = payload_len;
    state->rx_complete_last_response_len = state->tx_frame_len;
    state->rx_complete_last_response_preserves_replay = state->last_tx_preserved_replay;
    state->rx_complete_last_valid = true;
}

static bool zf_transport_nfc_is_duplicate_complete_i_block_locked(
    const ZfNfcTransportState *state, uint8_t request_pcb, const uint8_t *payload,
    size_t payload_len) {
    if (!state || !state->rx_complete_last_valid || (!payload && payload_len > 0U) ||
        payload_len != state->rx_complete_last_len || state->rx_complete_last_response_len == 0U ||
        state->rx_complete_last_response_len > sizeof(state->rx_complete_last_response) ||
        payload_len > sizeof(state->rx_complete_last_payload) ||
        request_pcb != state->rx_complete_last_pcb ||
        state->iso_cid_present != state->rx_complete_last_cid_present ||
        (state->iso_cid_present && state->iso_cid != state->rx_complete_last_cid)) {
        return false;
    }

    return payload_len == 0U ||
           memcmp(state->rx_complete_last_payload, payload, payload_len) == 0;
}

static bool zf_transport_nfc_replay_complete_i_block_response_locked(ZfNfcTransportState *state) {
    bool replay_valid = false;
    size_t replay_len = 0U;
    uint8_t replay[ZF_NFC_LAST_TX_CAPACITY];
    bool sent = false;

    if (!state || !state->rx_complete_last_valid ||
        state->rx_complete_last_response_len == 0U ||
        state->rx_complete_last_response_len > sizeof(state->rx_complete_last_response)) {
        return false;
    }

    replay_valid = state->iso4_last_tx_valid;
    replay_len = state->iso4_last_tx_len;
    memcpy(replay, state->iso4_last_tx, sizeof(replay));
    sent = zf_transport_nfc_send_frame(state, state->rx_complete_last_response,
                                       state->rx_complete_last_response_len);
    if (sent) {
        state->iso_pcb = state->rx_complete_last_iso_pcb;
    }
    if (state->rx_complete_last_response_preserves_replay) {
        state->iso4_last_tx_valid = replay_valid;
        state->iso4_last_tx_len = replay_len;
        memcpy(state->iso4_last_tx, replay, sizeof(state->iso4_last_tx));
    }
    return sent;
}

typedef enum {
    ZfNfcRxChainAppendOk = 0,
    ZfNfcRxChainAppendDuplicate = 1,
    ZfNfcRxChainAppendStalled = 2,
    ZfNfcRxChainAppendOverflow = 3,
} ZfNfcRxChainAppendResult;

static ZfNfcRxChainAppendResult
zf_transport_nfc_append_rx_chain_payload_locked(ZerofidoApp *app, ZfNfcTransportState *state,
                                                uint8_t request_pcb, const uint8_t *payload,
                                                size_t payload_len) {
    uint8_t *arena = zf_transport_nfc_arena(state);
    size_t old_request_len = 0U;
    size_t max_chain_len = 0U;

    if (!app || !state || !arena || (!payload && payload_len > 0U) ||
        state->request_len > ZF_MAX_MSG_SIZE) {
        return ZfNfcRxChainAppendOverflow;
    }

    max_chain_len = state->reader_profile.max_rx_chain_len != 0U
                        ? state->reader_profile.max_rx_chain_len
                        : ZF_MAX_MSG_SIZE;
    if (max_chain_len > ZF_MAX_MSG_SIZE) {
        max_chain_len = ZF_MAX_MSG_SIZE;
    }
    if (state->request_len > max_chain_len || payload_len > max_chain_len - state->request_len) {
        return ZfNfcRxChainAppendOverflow;
    }

    if (state->rx_chain_last_valid && state->rx_chain_last_pcb == request_pcb &&
        state->request_len == state->rx_chain_last_offset + state->rx_chain_last_len &&
        payload_len == state->rx_chain_last_len &&
        (payload_len == 0U ||
         memcmp(&arena[state->rx_chain_last_offset], payload, payload_len) == 0)) {
        const uint8_t duplicate_limit = state->reader_profile.rx_chain_duplicate_limit != 0U
                                            ? state->reader_profile.rx_chain_duplicate_limit
                                            : ZF_NFC_RX_CHAIN_DUPLICATE_LIMIT;
        if (state->rx_chain_duplicate_count >= duplicate_limit) {
            return ZfNfcRxChainAppendStalled;
        }
        state->rx_chain_duplicate_count++;
        return ZfNfcRxChainAppendDuplicate;
    }

    old_request_len = state->request_len;
    if (payload_len > 0U) {
        memcpy(&arena[state->request_len], payload, payload_len);
    }
    state->request_len += payload_len;
    state->command_chain_active = true;
    state->rx_chain_last_valid = true;
    state->rx_chain_last_pcb = request_pcb;
    state->rx_chain_duplicate_count = 0U;
    state->rx_chain_last_offset = old_request_len;
    state->rx_chain_last_len = payload_len;
    return ZfNfcRxChainAppendOk;
}

static void zf_transport_nfc_trace_apdu_after_response(const uint8_t *apdu_bytes, size_t apdu_len) {
    (void)apdu_bytes;
    (void)apdu_len;
}

static bool zf_transport_nfc_handle_native_desfire_locked(ZerofidoApp *app,
                                                          ZfNfcTransportState *state,
                                                          bool additional_frame) {
    static const uint8_t version_more[] = {
        ZF_NFC_DESFIRE_STATUS_MORE, 0x04, 0x01, 0x01, 0x01, 0x00, 0x18, 0x05,
    };
    static const uint8_t version_final[] = {
        ZF_NFC_DESFIRE_STATUS_OK,
        0x04,
        0xA1,
        0xB2,
        0xC3,
        0xD4,
        0xE5,
        0xF6,
        0x00,
        0x00,
        0x00,
        0x00,
        0x24,
        0x04,
        0x26,
    };

    if (!app || !state) {
        return false;
    }

    zf_transport_nfc_note_ui_stage_locked(
        state, state->applet_selected ? ZfNfcUiStageAppletSelected : ZfNfcUiStageAppletWaiting);
#if ZF_RELEASE_DIAGNOSTICS
    zf_transport_nfc_trace_event(
        !additional_frame
            ? "DESFire native version"
            : (state->desfire_probe_frame >= 2U ? "DESFire native done" : "DESFire native more"));
#else
    (void)app;
#endif
    if (!additional_frame) {
        /* Shim-local discovery progress only; do not touch FIDO applet/session state. */
        state->desfire_probe_frame = 1U;
        return zf_transport_nfc_send_iso_response_preserving_replay(state, version_more,
                                                                    sizeof(version_more), false);
    }
    if (state->desfire_probe_frame >= 2U) {
        /* Shim-local discovery progress only; do not touch FIDO applet/session state. */
        state->desfire_probe_frame = 0U;
        return zf_transport_nfc_send_iso_response_preserving_replay(state, version_final,
                                                                    sizeof(version_final), false);
    }

    /* Shim-local discovery progress only; do not touch FIDO applet/session state. */
    state->desfire_probe_frame = 2U;
    return zf_transport_nfc_send_iso_response_preserving_replay(state, version_more,
                                                                sizeof(version_more), false);
}

static bool zf_transport_nfc_send_ats(ZfNfcTransportState *state) {
    const Iso14443_4aAtsData *ats = state ? &state->iso14443_4a_data->ats_data : NULL;
    uint8_t response[32];
    size_t response_len = 0;

    if (!state || !ats || ats->tl == 0U || ats->tl > sizeof(response)) {
        return false;
    }

    response[response_len++] = ats->tl;
    if (ats->tl > 1U) {
        response[response_len++] = ats->t0;
        if ((ats->t0 & ZF_NFC_ATS_T0_TA1) != 0U) {
            response[response_len++] = ats->ta_1;
        }
        if ((ats->t0 & ZF_NFC_ATS_T0_TB1) != 0U) {
            response[response_len++] = ats->tb_1;
        }
        if ((ats->t0 & ZF_NFC_ATS_T0_TC1) != 0U) {
            response[response_len++] = ats->tc_1;
        }

        const uint32_t historical_len = simple_array_get_count(ats->t1_tk);
        if (historical_len > 0U && response_len + historical_len <= sizeof(response)) {
            memcpy(&response[response_len], simple_array_cget_data(ats->t1_tk), historical_len);
            response_len += historical_len;
        }
    }

    return response_len == ats->tl && zf_transport_nfc_send_frame(state, response, response_len);
}

static NfcCommand zf_transport_nfc_handle_rats_locked(ZerofidoApp *app, ZfNfcTransportState *state,
                                                      const uint8_t *frame, size_t frame_len) {
    bool sent = false;
    state->applet_selected = false;
    state->desfire_probe_frame = 0U;
    zf_transport_nfc_cancel_current_request_locked(state);
    zf_transport_nfc_reset_exchange_locked(state);
    zf_transport_nfc_clear_last_iso_response(state);
    zf_transport_nfc_clear_tx_chain(state);
    state->iso_pcb = ZF_NFC_PCB_BLOCK;
    state->iso_cid_present = false;
    state->iso_cid = 0U;
    state->iso4_active = true;
    zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
    sent = zf_transport_nfc_send_ats(state);
    zf_transport_nfc_trace_bytes("iso3-rx", frame, frame_len);
    furi_mutex_release(app->ui_mutex);
    zf_transport_nfc_set_frame_status(sent ? "NFC RATS" : "NFC RATS fail", frame[0], &frame[1],
                                      frame_len - 1U);
    return zf_transport_nfc_finish_unlocked(app, true, true, NfcCommandContinue);
}

static NfcCommand zf_transport_nfc_handle_poll_restart_locked(ZerofidoApp *app,
                                                              ZfNfcTransportState *state,
                                                              const uint8_t *frame,
                                                              size_t frame_len) {
    state->applet_selected = false;
    state->desfire_probe_frame = 0U;
    zf_transport_nfc_cancel_current_request_locked(state);
    zf_transport_nfc_reset_exchange_locked(state);
    zf_transport_nfc_clear_last_iso_response(state);
    zf_transport_nfc_clear_tx_chain(state);
    state->iso_pcb = ZF_NFC_PCB_BLOCK;
    state->iso_cid_present = false;
    state->iso_cid = 0U;
    state->iso4_active = false;
    zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
    UNUSED(frame_len);
    zf_transport_nfc_trace_bytes("iso3-rx", frame, frame_len);
    furi_mutex_release(app->ui_mutex);
    zf_transport_nfc_set_frame_status("NFC poll restart", frame[0], NULL, 0U);
    return zf_transport_nfc_finish_unlocked(app, true, true, NfcCommandReset);
}

static NfcCommand zf_transport_nfc_handle_iso4_payload_locked(
    ZerofidoApp *app, ZfNfcTransportState *state, const uint8_t *payload, size_t payload_len,
    uint8_t request_pcb, bool request_chained, bool refresh_status) {
    if (!app) {
        return NfcCommandContinue;
    }

    if (!state || (!payload && payload_len > 0U)) {
        furi_mutex_release(app->ui_mutex);
        return NfcCommandContinue;
    }

    if (!request_chained && !state->command_chain_active && payload_len == 0U) {
        return zf_transport_nfc_handle_empty_i_block_locked(app, state, request_pcb,
                                                            refresh_status);
    }

    if (!request_chained && !state->command_chain_active &&
        zf_transport_nfc_is_duplicate_complete_i_block_locked(state, request_pcb, payload,
                                                              payload_len)) {
        const bool replayed = zf_transport_nfc_replay_complete_i_block_response_locked(state);

        zf_transport_nfc_set_frame_status(replayed ? "NFC I-dup replay" : "NFC I-dup", request_pcb,
                                          payload, payload_len);
        furi_mutex_release(app->ui_mutex);
        return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
    }

    if (!request_chained && !state->command_chain_active &&
        zf_transport_nfc_should_sleep_post_success_probe_locked(state, payload, payload_len)) {
        zf_transport_nfc_set_frame_status("NFC probe sleep", request_pcb, payload, payload_len);
        return zf_transport_nfc_sleep_probe_locked(app, state, "NFC probe sleep");
    }

    if (!request_chained && !state->command_chain_active &&
        (zf_transport_nfc_is_native_desfire_get_version_payload(payload, payload_len) ||
         zf_transport_nfc_is_native_desfire_additional_frame(payload, payload_len))) {
        if (zf_transport_nfc_handle_native_desfire_locked(
                app, state, payload[0] == ZF_NFC_DESFIRE_CMD_ADDITIONAL_FRAME)) {
            zf_transport_nfc_remember_complete_i_block_locked(state, request_pcb, payload,
                                                              payload_len);
        }
        zf_transport_nfc_trace_bytes("native-rx", payload, payload_len);
        furi_mutex_release(app->ui_mutex);
        return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
    }

    if (zf_transport_nfc_request_worker_active(state)) {
        if (zf_transport_nfc_send_apdu_payload(state, (const uint8_t[]){ZF_NFC_STATUS_PROCESSING},
                                               1U, ZF_NFC_SW_STATUS_UPDATE)) {
            zf_transport_nfc_remember_complete_i_block_locked(state, request_pcb, payload,
                                                              payload_len);
        }
        furi_mutex_release(app->ui_mutex);
        return NfcCommandContinue;
    }

    if (!request_chained && !state->command_chain_active &&
        (state->processing || state->request_pending) && !state->response_ready &&
        payload_len >= 2U && payload[0] == 0x80U && payload[1] == ZF_NFC_INS_CTAP_MSG) {
        if (zf_transport_nfc_send_apdu_payload(state, (const uint8_t[]){ZF_NFC_STATUS_PROCESSING},
                                               1U, ZF_NFC_SW_STATUS_UPDATE)) {
            zf_transport_nfc_remember_complete_i_block_locked(state, request_pcb, payload,
                                                              payload_len);
        }
        furi_mutex_release(app->ui_mutex);
        return NfcCommandContinue;
    }

    if (request_chained) {
        ZfNfcRxChainAppendResult append_result = zf_transport_nfc_append_rx_chain_payload_locked(
            app, state, request_pcb, payload, payload_len);

        if (append_result == ZfNfcRxChainAppendDuplicate) {
            const uint8_t duplicate_count = state->rx_chain_duplicate_count;
            bool sent = false;

            furi_mutex_release(app->ui_mutex);
            if (duplicate_count == 1U) {
                zf_transport_nfc_set_frame_status("NFC I-chain dup", request_pcb, payload,
                                                  payload_len);
            }
            sent = zf_transport_nfc_send_r_ack(state, request_pcb);
            if (!sent) {
                zf_transport_nfc_set_frame_status("NFC I-chain dup fail", request_pcb, payload,
                                                  payload_len);
            }
            (void)refresh_status;
            return NfcCommandContinue;
        }

        if (append_result == ZfNfcRxChainAppendStalled) {
            zf_transport_nfc_reset_exchange_locked(state);
            zf_transport_nfc_clear_last_iso_response(state);
            zf_transport_nfc_clear_tx_chain(state);
            furi_mutex_release(app->ui_mutex);
            zf_transport_nfc_set_frame_status("NFC I-chain stall", request_pcb, payload,
                                              payload_len);
            return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandReset);
        }

        if (append_result == ZfNfcRxChainAppendOverflow) {
            zf_transport_nfc_reset_exchange_locked(state);
            furi_mutex_release(app->ui_mutex);
            zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
            return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
        }
        bool sent = false;

        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_set_frame_status("NFC I-chain", request_pcb, payload, payload_len);
        sent = zf_transport_nfc_send_r_ack(state, request_pcb);
        if (!sent) {
            zf_transport_nfc_set_frame_status("NFC I-chain fail", request_pcb, payload,
                                              payload_len);
        }
        (void)refresh_status;
        return NfcCommandContinue;
    }

    if (state->command_chain_active) {
        const uint8_t *assembled_frame = NULL;
        size_t assembled_len = 0U;
        if (state->request_len > ZF_MAX_MSG_SIZE ||
            payload_len > ZF_MAX_MSG_SIZE - state->request_len) {
            zf_transport_nfc_reset_exchange_locked(state);
            furi_mutex_release(app->ui_mutex);
            zf_transport_nfc_send_status_word(state, ZF_NFC_SW_WRONG_LENGTH);
            return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
        }

        if (payload_len > 0U) {
            memcpy(&app->transport_arena[state->request_len], payload, payload_len);
        }
        state->request_len += payload_len;
        assembled_frame = app->transport_arena;
        assembled_len = state->request_len;
        state->command_chain_active = false;
        state->request_len = 0U;
        state->rx_chain_last_valid = false;
        state->rx_chain_last_pcb = 0U;
        state->rx_chain_duplicate_count = 0U;
        state->rx_chain_last_offset = 0U;
        state->rx_chain_last_len = 0U;

        zf_transport_nfc_set_frame_status("NFC I-block", request_pcb, assembled_frame,
                                          assembled_len);
        if (zf_transport_nfc_handle_apdu_locked(app, state, assembled_frame, assembled_len)) {
            zf_transport_nfc_remember_complete_i_block_locked(state, request_pcb, payload,
                                                              payload_len);
        }
        zf_transport_nfc_trace_apdu_after_response(assembled_frame, assembled_len);
        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_trace_event("NFC I-block done");
        return NfcCommandContinue;
    }

    if (payload_len > 128U) {
        zf_transport_nfc_trace_event("NFC I-large");
    }
    if (!state->ctap_get_response_supported &&
        (state->processing || state->response_ready || state->response_is_error)) {
        zf_transport_nfc_set_frame_status("NFC deferred", request_pcb, payload, payload_len);
        if (zf_transport_nfc_send_deferred_response_or_wtx(app, state)) {
            zf_transport_nfc_remember_complete_i_block_locked(state, request_pcb, payload,
                                                              payload_len);
        }
        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_trace_event("NFC deferred done");
        return NfcCommandContinue;
    }
    zf_transport_nfc_set_frame_status("NFC I-block", request_pcb, payload, payload_len);
    if (zf_transport_nfc_handle_apdu_locked(app, state, payload, payload_len)) {
        zf_transport_nfc_remember_complete_i_block_locked(state, request_pcb, payload,
                                                          payload_len);
    }
    zf_transport_nfc_trace_apdu_after_response(payload, payload_len);
    furi_mutex_release(app->ui_mutex);
    zf_transport_nfc_trace_event("NFC I-block done");
    return NfcCommandContinue;
}

/*
 * Main NFC listener state machine. It interleaves ISO14443-3A discovery, RATS
 * and ATS setup, ISO14443-4A I/R/S block handling, bare APDU fallback, replay
 * recovery, and UI connection state updates.
 */
NfcCommand zf_transport_nfc_event_callback(NfcGenericEvent event, void *context) {
    ZerofidoApp *app = context;
    ZfNfcTransportState *state = zf_app_nfc_transport_state(app);
    const Iso14443_3aListenerEvent *iso3_event = NULL;
    const Iso14443_4aListenerEvent *iso4_event = NULL;
    const BitBuffer *rx_buffer = NULL;
    const uint8_t *frame = NULL;
    uint8_t frame_copy[2U + ZF_NFC_MAX_FRAME_INF_SIZE];
    size_t frame_len = 0;
    size_t payload_offset = 1U;
    bool refresh_status = false;
    bool protocol_3a = false;
    bool protocol_4a = false;
    bool standard_frame = false;

    if (!app || !state || !event.event_data) {
        return NfcCommandStop;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    const bool callback_allowed = state->listener_active && !state->stopping;
    furi_mutex_release(app->ui_mutex);
    if (!callback_allowed) {
        return NfcCommandStop;
    }

    if (event.protocol == NfcProtocolIso14443_3a) {
        protocol_3a = true;
        iso3_event = event.event_data;
    } else if (event.protocol == NfcProtocolIso14443_4a) {
        protocol_4a = true;
        iso4_event = event.event_data;
    } else {
        return NfcCommandStop;
    }

    if (protocol_3a) {
        switch (iso3_event->type) {
        case Iso14443_3aListenerEventTypeReceivedStandardFrame:
            standard_frame = true;
            if (iso3_event->data) {
                rx_buffer = iso3_event->data->buffer;
            }
            break;
        case Iso14443_3aListenerEventTypeReceivedData:
            if (iso3_event->data) {
                rx_buffer = iso3_event->data->buffer;
            }
            break;
        case Iso14443_3aListenerEventTypeHalted:
            zf_transport_nfc_trace_event("iso3-halted");
            zf_transport_nfc_on_disconnect(app);
            return NfcCommandSleep;
        case Iso14443_3aListenerEventTypeFieldOff:
            zf_transport_nfc_trace_event("iso3-field-off");
            zf_transport_nfc_on_disconnect(app);
            return NfcCommandSleep;
        default:
            return NfcCommandContinue;
        }
    } else {
        switch (iso4_event->type) {
        case Iso14443_4aListenerEventTypeReceivedData:
            if (iso4_event->data) {
                rx_buffer = iso4_event->data->buffer;
            }
            break;
        case Iso14443_4aListenerEventTypeHalted:
            zf_transport_nfc_trace_event("iso4-halted");
            zf_transport_nfc_on_disconnect(app);
            return NfcCommandSleep;
        case Iso14443_4aListenerEventTypeFieldOff:
            zf_transport_nfc_trace_event("iso4-field-off");
            zf_transport_nfc_on_disconnect(app);
            return NfcCommandSleep;
        default:
            return NfcCommandContinue;
        }
    }

    if (!rx_buffer) {
        return NfcCommandContinue;
    }

    frame_len = bit_buffer_get_size_bytes(rx_buffer);
    if (frame_len == 0 || frame_len > (2U + ZF_NFC_MAX_FRAME_INF_SIZE)) {
        return NfcCommandContinue;
    }
    frame = bit_buffer_get_data(rx_buffer);
    if (!frame) {
        return NfcCommandContinue;
    }
    memcpy(frame_copy, frame, frame_len);
    frame = frame_copy;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (protocol_4a && event.instance) {
        state->iso4_listener = (Iso14443_4aListener *)event.instance;
    } else if (protocol_3a) {
        state->iso4_listener = NULL;
    }
    refresh_status = !state->field_active || !state->iso4_active;
    state->field_active = true;

    if (frame_len == 1U && zf_transport_nfc_is_one_byte_recovery_r_nak(frame[0])) {
        return zf_transport_nfc_handle_recovery_r_nak_locked(app, state, frame, frame_len,
                                                             refresh_status);
    }

    if (protocol_3a && frame_len == 1U &&
        (frame[0] == ZF_NFC_REQA_CMD || frame[0] == ZF_NFC_WUPA_CMD)) {
        return zf_transport_nfc_handle_poll_restart_locked(app, state, frame, frame_len);
    }

    if (protocol_3a && standard_frame && frame_len == 2U && frame[0] == ZF_NFC_RATS_CMD) {
        return zf_transport_nfc_handle_rats_locked(app, state, frame, frame_len);
    }

    if (protocol_3a && !state->iso4_active) {
        furi_mutex_release(app->ui_mutex);
        return NfcCommandContinue;
    }

    state->iso4_active = true;
    if (state->applet_selected) {
        if (state->last_visible_stage != ZfNfcUiStageAppletSelected) {
            zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletSelected);
            refresh_status = true;
        }
    } else if (state->last_visible_stage != ZfNfcUiStageAppletWaiting) {
        zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
        refresh_status = true;
    }
    if (zf_transport_nfc_should_sleep_post_success_probe_locked(state, frame, frame_len)) {
        zf_transport_nfc_set_frame_status("NFC probe sleep", frame[0], NULL, 0U);
        return zf_transport_nfc_sleep_probe_locked(app, state, "NFC probe sleep");
    }

    if (frame_len == 1U && (frame[0] == ZF_NFC_DESFIRE_CMD_GET_VERSION ||
                            frame[0] == ZF_NFC_DESFIRE_CMD_ADDITIONAL_FRAME)) {
        zf_transport_nfc_handle_native_desfire_locked(
            app, state, frame[0] == ZF_NFC_DESFIRE_CMD_ADDITIONAL_FRAME);
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        furi_mutex_release(app->ui_mutex);
        return zf_transport_nfc_finish_unlocked(app, true, true, NfcCommandContinue);
    }

    if (zf_transport_nfc_is_bare_apdu(frame, frame_len)) {
        if (zf_transport_nfc_should_sleep_post_success_probe_locked(state, frame, frame_len)) {
            zf_transport_nfc_set_frame_status("NFC probe sleep", frame[0], NULL, 0U);
            return zf_transport_nfc_sleep_probe_locked(app, state, "NFC probe sleep");
        }
        state->iso_cid_present = false;
        zf_transport_nfc_handle_apdu_locked(app, state, frame, frame_len);
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        zf_transport_nfc_trace_apdu_after_response(frame, frame_len);
        furi_mutex_release(app->ui_mutex);
        return zf_transport_nfc_finish_unlocked(app, true, true, NfcCommandContinue);
    }

    if (zf_transport_nfc_is_pps_frame(frame, frame_len)) {
        const bool pps_defer = zf_transport_nfc_has_large_cached_replay(state);
        bool sent = false;

        if (!pps_defer) {
            sent = zf_transport_nfc_send_pps_ack(state, frame[0]);
        }
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_set_frame_status(pps_defer ? "NFC PPS defer"
                                                    : (sent ? "NFC PPS ack" : "NFC PPS fail"),
                                          frame[0], &frame[1], frame_len - 1U);
        return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
    }

    if (zf_transport_nfc_is_r_block(frame[0])) {
        const bool r_nack = zf_transport_nfc_is_r_nack_block(frame[0]);
        bool chain_ack_sent = false;
        bool sent = false;

        if (r_nack) {
            sent = zf_transport_nfc_replay_last_iso_response(state);
        } else if (state->iso4_tx_chain_active) {
            sent = zf_transport_nfc_send_next_tx_chain_block(state);
            chain_ack_sent = sent;
        } else if (state->iso4_tx_chain_completed) {
            zf_transport_nfc_ack_completed_tx_chain_locked(state);
        }
        if (!chain_ack_sent) {
            zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        }
        furi_mutex_release(app->ui_mutex);
        if (!chain_ack_sent) {
            zf_transport_nfc_set_frame_status(r_nack ? (sent ? "NFC R-NAK replay" : "NFC R-NAK")
                                                     : "NFC R-ACK",
                                              frame[0], &frame[1], frame_len - 1U);
        }
        return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
    }

    if (frame_len == 1U && zf_transport_nfc_is_i_block(frame[0])) {
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        return zf_transport_nfc_handle_empty_i_block_locked(app, state, frame[0], refresh_status);
    }

    if (zf_transport_nfc_is_s_block(frame[0])) {
        const bool s_deselect = zf_transport_nfc_is_s_deselect_block(frame[0]);
        const bool s_wtx = zf_transport_nfc_is_s_wtx_block(frame[0]);

        if (s_deselect) {
            zf_transport_nfc_teardown_rf_activation_locked(state);
            zf_transport_nfc_note_ui_stage_locked(state, ZfNfcUiStageAppletWaiting);
            zf_transport_nfc_send_frame(state, (const uint8_t[]){0xC2U}, 1U);
        }
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_set_frame_status(s_deselect ? "NFC S-deselect"
                                                     : (s_wtx ? "NFC S-wtx" : "NFC S-block"),
                                          frame[0], &frame[1], frame_len - 1U);
        if (s_deselect) {
            zerofido_ui_cancel_pending_interaction(app);
        }
        return zf_transport_nfc_finish_unlocked(app, !s_deselect, true,
                                                s_deselect ? NfcCommandSleep : NfcCommandContinue);
    }

    if (!zf_transport_nfc_is_i_block(frame[0])) {
        if ((frame[0] & ZF_NFC_PCB_CID) == 0U) {
            zf_transport_nfc_send_r_nak(state, frame[0]);
        }
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_set_frame_status("NFC block", frame[0], &frame[1], frame_len - 1U);
        return zf_transport_nfc_finish_unlocked(app, true, refresh_status, NfcCommandContinue);
    }

    if (!zf_transport_nfc_sync_i_block_header(state, frame, frame_len, &payload_offset)) {
        if ((frame[0] & ZF_NFC_PCB_CID) == 0U) {
            zf_transport_nfc_send_r_nak(state, frame[0]);
        }
        zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
        furi_mutex_release(app->ui_mutex);
        zf_transport_nfc_set_frame_status("NFC I malformed", frame[0], NULL, 0U);
        return NfcCommandContinue;
    }
    zf_transport_nfc_trace_bytes("iso-rx", frame, frame_len);
    return zf_transport_nfc_handle_iso4_payload_locked(
        app, state, &frame[payload_offset], frame_len - payload_offset, frame[0],
        (frame[0] & ZF_NFC_PCB_CHAIN) != 0U, refresh_status);
}

#endif
