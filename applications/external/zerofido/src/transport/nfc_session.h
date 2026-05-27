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

#include "nfc_protocol.h"
#include "nfc_worker.h"
#include "../zerofido_runtime_config.h"

/*
 * NFC session state bridges ISO-DEP/APDU exchange with the shared protocol
 * dispatcher. Requests are queued into the app transport arena, processed by
 * the worker thread, and paged back through GET RESPONSE or status polling.
 */
void zf_transport_nfc_attach_arena(ZfNfcTransportState *state, uint8_t *arena,
                                   size_t arena_capacity);
uint8_t *zf_transport_nfc_arena(const ZfNfcTransportState *state);
size_t zf_transport_nfc_arena_capacity(const ZfNfcTransportState *state);
uint32_t zf_transport_nfc_next_session_id(ZfTransportSessionId current);
void zf_transport_nfc_note_ui_stage_locked(ZfNfcTransportState *state, ZfNfcUiStage stage);
bool zf_transport_nfc_request_worker_active(const ZfNfcTransportState *state);
void zf_transport_nfc_reset_exchange_locked(ZfNfcTransportState *state);
void zf_transport_nfc_teardown_rf_activation_locked(ZfNfcTransportState *state);
void zf_transport_nfc_cancel_current_request_locked(ZfNfcTransportState *state);
void zf_transport_nfc_on_disconnect(ZerofidoApp *app);
uint8_t zf_transport_nfc_current_status(const ZerofidoApp *app);
bool zf_transport_nfc_send_get_response(const ZerofidoApp *app, ZfNfcTransportState *state,
                                        const ZfNfcApdu *apdu);
bool zf_transport_nfc_send_deferred_response_or_wtx(const ZerofidoApp *app,
                                                    ZfNfcTransportState *state);
bool zf_transport_nfc_queue_request_locked(ZerofidoApp *app, ZfNfcTransportState *state,
                                           ZfNfcRequestKind request_kind, const uint8_t *request,
                                           size_t request_len);
bool zf_transport_nfc_handle_select(ZfNfcTransportState *state, const ZfNfcApdu *apdu,
                                    const ZfResolvedCapabilities *capabilities);
void zf_transport_nfc_store_response(ZerofidoApp *app, ZfNfcTransportState *state,
                                     ZfTransportSessionId session_id, uint32_t request_generation,
                                     const uint8_t *response, size_t response_len,
                                     bool response_is_u2f, bool response_is_error,
                                     uint16_t error_status_word);
