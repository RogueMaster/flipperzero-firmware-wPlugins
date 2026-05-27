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

#include "nfc_iso_dep.h"

#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <nfc/helpers/iso14443_crc.h>
#include <lib/toolbox/simple_array.h>
#include <string.h>
#include <toolbox/bit_buffer.h>

#include "nfc_protocol.h"
#include "nfc_trace.h"

static ZfNfcReaderProfile zf_transport_nfc_default_reader_profile(ZfNfcReaderProfileKind kind) {
    ZfNfcReaderProfile profile = {
        .kind = kind,
        .ats = {0x05U, 0x78U, 0x91U, 0xE8U, 0x00U},
        .ats_len = 5U,
        .r_ack_uses_incoming_block = true,
        .rx_chain_duplicate_limit = 8U,
        .max_rx_chain_len = ZF_MAX_MSG_SIZE,
    };

    /*
     * Android is intentionally profile-compatible for now. Keeping it explicit
     * prevents iOS-specific behavior from becoming hard-coded into the engine.
     */
    if (kind == ZfNfcReaderProfileAndroid) {
        profile.kind = ZfNfcReaderProfileAndroid;
    }
    return profile;
}

void zf_transport_nfc_set_reader_profile(ZfNfcTransportState *state, ZfNfcReaderProfileKind kind) {
    if (!state) {
        return;
    }

    state->reader_profile = zf_transport_nfc_default_reader_profile(kind);
}

static void zf_transport_nfc_ensure_reader_profile(ZfNfcTransportState *state) {
    if (!state || state->reader_profile.ats_len != 0U) {
        return;
    }

    zf_transport_nfc_set_reader_profile(state, ZfNfcReaderProfileIos);
}

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
static void zf_transport_nfc_trace_cache_state(const char *event, const ZfNfcTransportState *state,
                                               uint8_t pcb, size_t data_len) {
    if (state && state->iso4_tx_chain_active) {
        return;
    }

    zf_transport_nfc_trace_format(
        "iso-cache %s pcb=%02X len=%u valid=%u cached=%u frame=%u", event, pcb, (unsigned)data_len,
        state && state->iso4_last_tx_valid ? 1U : 0U,
        state ? (unsigned)state->iso4_last_tx_len : 0U, state ? (unsigned)state->tx_frame_len : 0U);
}
#else
static void zf_transport_nfc_trace_cache_state(const char *event, const ZfNfcTransportState *state,
                                               uint8_t pcb, size_t data_len) {
    (void)event;
    (void)state;
    (void)pcb;
    (void)data_len;
}
#endif

static void zf_transport_nfc_cache_last_iso_response(ZfNfcTransportState *state,
                                                     const uint8_t *data, size_t data_len) {
    if (!state || (!data && data_len > 0U) || data_len > sizeof(state->iso4_last_tx)) {
        zf_transport_nfc_trace_cache_state("skip", state, data && data_len > 0U ? data[0] : 0U,
                                           data_len);
        return;
    }

    if (data_len > 0U) {
        memmove(state->iso4_last_tx, data, data_len);
    }
    state->iso4_last_tx_len = data_len;
    state->iso4_last_tx_valid = true;
    zf_transport_nfc_trace_cache_state("set", state, data_len > 0U ? data[0] : 0U, data_len);
}

static bool zf_transport_nfc_is_replayable_iso_i_response(const uint8_t *data, size_t data_len) {
    if (!data || data_len < 2U) {
        return false;
    }

    return (data[0] & 0xC2U) == ZF_NFC_PCB_BLOCK;
}

typedef struct {
    bool iso4_last_tx_valid;
    size_t iso4_last_tx_len;
    uint8_t iso4_last_tx[ZF_NFC_LAST_TX_CAPACITY];
} ZfNfcReplaySnapshot;

static void zf_transport_nfc_snapshot_replay(ZfNfcTransportState *state,
                                             ZfNfcReplaySnapshot *snapshot) {
    if (!snapshot) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    if (!state) {
        return;
    }

    snapshot->iso4_last_tx_valid = state->iso4_last_tx_valid;
    snapshot->iso4_last_tx_len = state->iso4_last_tx_len;
    memcpy(snapshot->iso4_last_tx, state->iso4_last_tx, sizeof(snapshot->iso4_last_tx));
}

static void zf_transport_nfc_restore_replay(ZfNfcTransportState *state,
                                            const ZfNfcReplaySnapshot *snapshot) {
    if (!state || !snapshot) {
        return;
    }

    state->iso4_last_tx_valid = snapshot->iso4_last_tx_valid;
    state->iso4_last_tx_len = snapshot->iso4_last_tx_len;
    memcpy(state->iso4_last_tx, snapshot->iso4_last_tx, sizeof(state->iso4_last_tx));
}

static bool zf_transport_nfc_snapshot_has_replay(const ZfNfcReplaySnapshot *snapshot) {
    if (!snapshot) {
        return false;
    }

    return snapshot->iso4_last_tx_valid || snapshot->iso4_last_tx_len > 0U;
}

bool zf_transport_nfc_send_frame(ZfNfcTransportState *state, const uint8_t *data, size_t data_len) {
    const uint8_t *tx_data = NULL;

    if (!state || !state->nfc || !state->tx_buffer || (!data && data_len > 0U) ||
        data_len > sizeof(state->iso4_tx_frame)) {
        return false;
    }

    state->tx_frame_len = data_len;
    if (data_len > 0U) {
        memmove(state->iso4_tx_frame, data, data_len);
    }
    tx_data = state->iso4_tx_frame;
    bit_buffer_reset(state->tx_buffer);
    bit_buffer_append_bytes(state->tx_buffer, tx_data, data_len);
    iso14443_crc_append(Iso14443CrcTypeA, state->tx_buffer);
    const bool sent = nfc_listener_tx(state->nfc, state->tx_buffer) == NfcErrorNone;
    if (sent) {
        state->last_tx_preserved_replay = false;
        if (zf_transport_nfc_is_replayable_iso_i_response(tx_data, data_len)) {
            zf_transport_nfc_cache_last_iso_response(state, tx_data, data_len);
        }
        if (!state->iso4_tx_chain_active) {
            zf_transport_nfc_trace_bytes("iso-tx", tx_data, data_len);
        }
    }
    return sent;
}

bool zf_transport_nfc_send_raw_bits(ZfNfcTransportState *state, const uint8_t *data,
                                    size_t bit_len) {
    const size_t byte_len = (bit_len + 7U) / 8U;

    if (!state || !state->nfc || !state->tx_buffer || (!data && bit_len > 0U)) {
        return false;
    }

    bit_buffer_reset(state->tx_buffer);
    bit_buffer_set_size(state->tx_buffer, bit_len);
    for (size_t i = 0; i < byte_len; ++i) {
        bit_buffer_set_byte(state->tx_buffer, i, data[i]);
    }
    return nfc_listener_tx(state->nfc, state->tx_buffer) == NfcErrorNone;
}

bool zf_transport_nfc_send_short_frame(ZfNfcTransportState *state, uint8_t data) {
    return zf_transport_nfc_send_raw_bits(state, &data, 4U);
}

/*
 * Encodes one ISO-DEP I-block response. PCB/CID are synthesized from the active
 * ISO state, the chaining bit is set when more data follows, and the PCB toggle
 * advances only after a successful send.
 */
bool zf_transport_nfc_send_iso_response(ZfNfcTransportState *state, const uint8_t *data,
                                        size_t data_len, bool chaining) {
    uint8_t block[ZF_NFC_MAX_TX_FRAME_INF_SIZE + 2U];
    size_t block_len = 0U;
    uint8_t pcb = 0U;
    bool sent = false;

    if (!state || (!data && data_len > 0U) || data_len > ZF_NFC_MAX_TX_FRAME_INF_SIZE) {
        return false;
    }

    pcb = (uint8_t)(ZF_NFC_PCB_BLOCK | (state->iso_pcb & 0x01U) |
                    (state->iso_cid_present ? ZF_NFC_PCB_CID : 0U) |
                    (chaining ? ZF_NFC_PCB_CHAIN : 0U));
    block[block_len++] = pcb;
    if (state->iso_cid_present) {
        block[block_len++] = state->iso_cid;
    }
    if (data_len > 0) {
        memcpy(&block[block_len], data, data_len);
        block_len += data_len;
    }
    sent = zf_transport_nfc_send_frame(state, block, block_len);
    if (sent) {
        state->iso_pcb ^= 0x01U;
        if (!state->iso4_tx_chain_active) {
            zf_transport_nfc_trace_bytes("apdu-tx", data, data_len);
        }
    }
    return sent;
}

/*
 * Sends shim/discovery responses without overwriting the replay cache. DESFire
 * probes and similar reader discovery traffic must not replace the last FIDO
 * ISO response used for R-NAK/empty-I recovery.
 */
bool zf_transport_nfc_send_iso_response_preserving_replay(ZfNfcTransportState *state,
                                                          const uint8_t *data, size_t data_len,
                                                          bool chaining) {
    ZfNfcReplaySnapshot snapshot;
    bool restore_replay = false;
    bool sent = false;

    zf_transport_nfc_snapshot_replay(state, &snapshot);
    restore_replay = zf_transport_nfc_snapshot_has_replay(&snapshot);
    sent = zf_transport_nfc_send_iso_response(state, data, data_len, chaining);
    if (restore_replay) {
        zf_transport_nfc_restore_replay(state, &snapshot);
    }
    if (sent) {
        state->last_tx_preserved_replay = true;
    }
    return sent;
}

bool zf_transport_nfc_send_r_ack(ZfNfcTransportState *state, uint8_t pcb) {
    uint8_t block_number = (pcb & 0x01U);

    zf_transport_nfc_ensure_reader_profile(state);
    if (state && !state->reader_profile.r_ack_uses_incoming_block) {
        block_number ^= 0x01U;
    }
    const uint8_t ack[2] = {
        (uint8_t)(ZF_NFC_PCB_R_BLOCK | block_number | ZF_NFC_PCB_BLOCK |
                  (state && state->iso_cid_present ? ZF_NFC_PCB_CID : 0U)),
        state ? state->iso_cid : 0U,
    };
    return zf_transport_nfc_send_frame(state, ack, state && state->iso_cid_present ? 2U : 1U);
}

bool zf_transport_nfc_send_r_nak(ZfNfcTransportState *state, uint8_t pcb) {
    const uint8_t nak[2] = {
        (uint8_t)(ZF_NFC_PCB_R_BLOCK | (pcb & 0x01U) | ZF_NFC_PCB_BLOCK | ZF_NFC_PCB_CHAIN |
                  (state && state->iso_cid_present ? ZF_NFC_PCB_CID : 0U)),
        state ? state->iso_cid : 0U,
    };
    return zf_transport_nfc_send_frame(state, nak, state && state->iso_cid_present ? 2U : 1U);
}

bool zf_transport_nfc_send_wtx(ZfNfcTransportState *state) {
    uint8_t frame[3] = {0};
    size_t frame_len = 0U;

    if (!state) {
        return false;
    }

    frame[frame_len++] = (uint8_t)(0xF2U | (state->iso_cid_present ? ZF_NFC_PCB_CID : 0U));
    if (state->iso_cid_present) {
        frame[frame_len++] = state->iso_cid;
    }
    frame[frame_len++] = 0x0AU;
    return zf_transport_nfc_send_frame(state, frame, frame_len);
}

void zf_transport_nfc_clear_last_iso_response(ZfNfcTransportState *state) {
    bool had_cached_response = false;

    if (!state) {
        return;
    }

    had_cached_response = state->iso4_last_tx_valid || state->iso4_last_tx_len > 0U;
    state->iso4_last_tx_valid = false;
    state->iso4_last_tx_len = 0U;
    memset(state->iso4_last_tx, 0, sizeof(state->iso4_last_tx));
    if (had_cached_response) {
        zf_transport_nfc_trace_cache_state("clear", state, 0U, 0U);
    }
}

/*
 * Replays the last cacheable ISO I-block for reader recovery paths such as
 * R-NAK or empty I-blocks. The cache is protocol recovery state, not a queued
 * application response.
 */
bool zf_transport_nfc_replay_last_iso_response(ZfNfcTransportState *state) {
    if (!state) {
        zf_transport_nfc_trace_cache_state("replay-null", state, 0U, 0U);
        return false;
    }

    if (state->iso4_last_tx_valid && state->iso4_last_tx_len > 0U &&
        state->iso4_last_tx_len <= sizeof(state->iso4_last_tx)) {
        bool sent = false;

        sent = zf_transport_nfc_send_frame(state, state->iso4_last_tx, state->iso4_last_tx_len);
        zf_transport_nfc_trace_cache_state(sent ? "replay-array" : "replay-array-fail", state,
                                           state->iso4_last_tx[0], state->iso4_last_tx_len);
        return sent;
    }

    zf_transport_nfc_trace_cache_state("replay-miss", state, 0U, 0U);
    return false;
}

void zf_transport_nfc_clear_tx_chain(ZfNfcTransportState *state) {
    if (!state) {
        return;
    }

    state->iso4_tx_chain_active = false;
    state->iso4_tx_chain_completed = false;
    state->iso4_tx_chain_data = NULL;
    state->iso4_tx_chain_len = 0U;
    state->iso4_tx_chain_offset = 0U;
    state->iso4_tx_chain_status_word = 0U;
}

static void zf_transport_nfc_build_tx_chain_chunk(const ZfNfcTransportState *state, uint8_t *chunk,
                                                  size_t chunk_len) {
    const size_t payload_len = state->iso4_tx_chain_len - 2U;
    size_t absolute_offset = state->iso4_tx_chain_offset;

    for (size_t i = 0U; i < chunk_len; ++i, ++absolute_offset) {
        if (absolute_offset < payload_len) {
            chunk[i] = state->iso4_tx_chain_data[absolute_offset];
        } else {
            const size_t sw_offset = absolute_offset - payload_len;
            chunk[i] = sw_offset == 0U ? (uint8_t)(state->iso4_tx_chain_status_word >> 8)
                                       : (uint8_t)state->iso4_tx_chain_status_word;
        }
    }
}

bool zf_transport_nfc_send_next_tx_chain_block(ZfNfcTransportState *state) {
    uint8_t chunk[ZF_NFC_TX_CHAIN_CHUNK_SIZE];
    size_t remaining = 0U;
    size_t chunk_len = 0U;
    bool chaining = false;
    bool sent = false;

    if (!state || !state->iso4_tx_chain_active || state->iso4_tx_chain_len < 2U ||
        state->iso4_tx_chain_len > ZF_TRANSPORT_ARENA_SIZE ||
        state->iso4_tx_chain_offset >= state->iso4_tx_chain_len ||
        (!state->iso4_tx_chain_data && state->iso4_tx_chain_len > 2U)) {
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
        zf_transport_nfc_trace_format("tx-chain invalid active=%u len=%u off=%u data=%u arena=%u",
                                      state && state->iso4_tx_chain_active ? 1U : 0U,
                                      state ? (unsigned)state->iso4_tx_chain_len : 0U,
                                      state ? (unsigned)state->iso4_tx_chain_offset : 0U,
                                      state && state->iso4_tx_chain_data ? 1U : 0U,
                                      (unsigned)ZF_TRANSPORT_ARENA_SIZE);
#endif
        return false;
    }

    remaining = state->iso4_tx_chain_len - state->iso4_tx_chain_offset;
    chunk_len = remaining > ZF_NFC_TX_CHAIN_CHUNK_SIZE ? ZF_NFC_TX_CHAIN_CHUNK_SIZE : remaining;
    chaining = remaining > chunk_len;
    zf_transport_nfc_build_tx_chain_chunk(state, chunk, chunk_len);
    sent = zf_transport_nfc_send_iso_response(state, chunk, chunk_len, chaining);
    if (!sent) {
#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
        zf_transport_nfc_trace_format("tx-chain send fail len=%u rem=%u chain=%u",
                                      (unsigned)chunk_len, (unsigned)remaining, chaining ? 1U : 0U);
#endif
        return false;
    }

    state->iso4_tx_chain_offset += chunk_len;
    if (state->iso4_tx_chain_offset >= state->iso4_tx_chain_len) {
        state->iso4_tx_chain_active = false;
        state->iso4_tx_chain_completed = true;
        state->iso4_tx_chain_data = NULL;
        state->iso4_tx_chain_len = 0U;
        state->iso4_tx_chain_offset = 0U;
        state->iso4_tx_chain_status_word = 0U;
    }
    return true;
}

bool zf_transport_nfc_begin_chained_apdu_payload(ZfNfcTransportState *state, const uint8_t *data,
                                                 size_t data_len, uint16_t status_word) {
    size_t total_len = 0U;

    if (!state || (!data && data_len > 0U) || data_len > ZF_TRANSPORT_ARENA_SIZE - 2U ||
        data_len + 2U > ZF_TRANSPORT_ARENA_SIZE) {
        return false;
    }

    total_len = data_len + 2U;
    zf_transport_nfc_clear_tx_chain(state);
    state->iso4_tx_chain_data = data;
    state->iso4_tx_chain_len = total_len;
    state->iso4_tx_chain_offset = 0U;
    state->iso4_tx_chain_status_word = status_word;
    state->iso4_tx_chain_active = true;
    return zf_transport_nfc_send_next_tx_chain_block(state);
}

bool zf_transport_nfc_send_status_word(ZfNfcTransportState *state, uint16_t status_word) {
    const uint8_t bytes[2] = {(uint8_t)(status_word >> 8), (uint8_t)status_word};

    zf_transport_nfc_clear_tx_chain(state);
    return zf_transport_nfc_send_iso_response(state, bytes, sizeof(bytes), false);
}

bool zf_transport_nfc_send_forced_iso_status_word(ZfNfcTransportState *state,
                                                  uint16_t status_word) {
    if (!state) {
        return false;
    }

    return zf_transport_nfc_send_status_word(state, status_word);
}

bool zf_transport_nfc_send_apdu_payload(ZfNfcTransportState *state, const uint8_t *data,
                                        size_t data_len, uint16_t status_word) {
    size_t frame_len = data_len + 2U;
    uint8_t block[ZF_NFC_MAX_TX_FRAME_INF_SIZE + 2U];
    size_t block_len = 0U;
    uint8_t pcb = 0U;
    uint8_t sw[2] = {(uint8_t)(status_word >> 8), (uint8_t)status_word};
    bool sent = false;

    if (!state || (!data && data_len > 0U) || frame_len > ZF_NFC_MAX_TX_FRAME_INF_SIZE) {
        return false;
    }

    zf_transport_nfc_clear_tx_chain(state);
    pcb = (uint8_t)(ZF_NFC_PCB_BLOCK | (state->iso_pcb & 0x01U) |
                    (state->iso_cid_present ? ZF_NFC_PCB_CID : 0U));
    block[block_len++] = pcb;
    if (state->iso_cid_present) {
        block[block_len++] = state->iso_cid;
    }
    if (data_len > 0U) {
        memcpy(&block[block_len], data, data_len);
        block_len += data_len;
    }
    memcpy(&block[block_len], sw, sizeof(sw));
    block_len += sizeof(sw);
    sent = zf_transport_nfc_send_frame(state, block, block_len);
    if (sent) {
        state->iso_pcb ^= 0x01U;
        zf_transport_nfc_trace_apdu_tx(data, data_len, status_word);
    }
    return sent;
}

bool zf_transport_nfc_send_apdu_payload_preserving_replay(ZfNfcTransportState *state,
                                                          const uint8_t *data, size_t data_len,
                                                          uint16_t status_word) {
    ZfNfcReplaySnapshot snapshot;
    bool restore_replay = false;
    bool sent = false;

    zf_transport_nfc_snapshot_replay(state, &snapshot);
    restore_replay = zf_transport_nfc_snapshot_has_replay(&snapshot);
    sent = zf_transport_nfc_send_apdu_payload(state, data, data_len, status_word);
    if (restore_replay) {
        zf_transport_nfc_restore_replay(state, &snapshot);
    }
    if (sent) {
        state->last_tx_preserved_replay = true;
    }
    return sent;
}

void zf_transport_nfc_prepare_listener(ZfNfcTransportState *state) {
    static const uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    static const uint8_t atqa[] = {0x44, 0x00};
    Iso14443_3aData *base_data = NULL;
    const ZfNfcReaderProfile *profile = NULL;

    zf_transport_nfc_ensure_reader_profile(state);
    profile = &state->reader_profile;
    iso14443_4a_reset(state->iso14443_4a_data);
    iso14443_4a_set_uid(state->iso14443_4a_data, uid, sizeof(uid));
    base_data = iso14443_4a_get_base_data(state->iso14443_4a_data);
    iso14443_3a_set_atqa(base_data, atqa);
    iso14443_3a_set_sak(base_data, 0x20U);
    state->iso14443_4a_data->ats_data.tl = profile->ats_len >= 1U ? profile->ats[0] : 0x05U;
    state->iso14443_4a_data->ats_data.t0 = profile->ats_len >= 2U ? profile->ats[1] : 0x78U;
    state->iso14443_4a_data->ats_data.ta_1 = profile->ats_len >= 3U ? profile->ats[2] : 0x91U;
    state->iso14443_4a_data->ats_data.tb_1 = profile->ats_len >= 4U ? profile->ats[3] : 0xE8U;
    state->iso14443_4a_data->ats_data.tc_1 = profile->ats_len >= 5U ? profile->ats[4] : 0x00U;
    simple_array_reset(state->iso14443_4a_data->ats_data.t1_tk);
}

#endif
