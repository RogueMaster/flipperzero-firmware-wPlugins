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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nfc_worker.h"

#define ZF_NFC_PCB_BLOCK 0x02U
#define ZF_NFC_PCB_CID 0x08U
#define ZF_NFC_PCB_CHAIN 0x10U
#define ZF_NFC_PCB_R_BLOCK 0xA0U
#define ZF_NFC_PCB_S_BLOCK 0xC0U

/*
 * ISO-DEP transmit helpers own response framing, replay of the last I-block,
 * R-ACK handling, and APDU payload chaining above the raw NFC listener.
 */
bool zf_transport_nfc_send_frame(ZfNfcTransportState *state, const uint8_t *data, size_t data_len);
bool zf_transport_nfc_send_raw_bits(ZfNfcTransportState *state, const uint8_t *data,
                                    size_t bit_len);
bool zf_transport_nfc_send_short_frame(ZfNfcTransportState *state, uint8_t data);
bool zf_transport_nfc_send_iso_response(ZfNfcTransportState *state, const uint8_t *data,
                                        size_t data_len, bool chaining);
bool zf_transport_nfc_send_iso_response_preserving_replay(ZfNfcTransportState *state,
                                                          const uint8_t *data, size_t data_len,
                                                          bool chaining);
bool zf_transport_nfc_send_r_ack(ZfNfcTransportState *state, uint8_t pcb);
bool zf_transport_nfc_send_r_nak(ZfNfcTransportState *state, uint8_t pcb);
bool zf_transport_nfc_send_wtx(ZfNfcTransportState *state);
void zf_transport_nfc_clear_last_iso_response(ZfNfcTransportState *state);
bool zf_transport_nfc_replay_last_iso_response(ZfNfcTransportState *state);
void zf_transport_nfc_clear_tx_chain(ZfNfcTransportState *state);
bool zf_transport_nfc_begin_chained_apdu_payload(ZfNfcTransportState *state, const uint8_t *data,
                                                 size_t data_len, uint16_t status_word);
bool zf_transport_nfc_send_next_tx_chain_block(ZfNfcTransportState *state);
bool zf_transport_nfc_send_status_word(ZfNfcTransportState *state, uint16_t status_word);
bool zf_transport_nfc_send_forced_iso_status_word(ZfNfcTransportState *state, uint16_t status_word);
bool zf_transport_nfc_send_apdu_payload(ZfNfcTransportState *state, const uint8_t *data,
                                        size_t data_len, uint16_t status_word);
bool zf_transport_nfc_send_apdu_payload_preserving_replay(ZfNfcTransportState *state,
                                                          const uint8_t *data, size_t data_len,
                                                          uint16_t status_word);
void zf_transport_nfc_set_reader_profile(ZfNfcTransportState *state, ZfNfcReaderProfileKind kind);
void zf_transport_nfc_prepare_listener(ZfNfcTransportState *state);
