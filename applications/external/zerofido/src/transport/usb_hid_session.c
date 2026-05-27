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

#ifndef ZF_NFC_ONLY

#include "usb_hid_session.h"

#include <furi.h>
#include <furi_hal_usb_hid_u2f.h>
#include <string.h>

#include "dispatch.h"
#include "../u2f/apdu.h"
#include "../zerofido_app_i.h"
#include "../zerofido_crypto.h"

#define ZF_U2F_HID_RESPONSE_SETTLE_MS 50U

static void zf_transport_handle_init(uint32_t response_cid, uint32_t assigned_cid,
                                     const uint8_t *payload, size_t payload_len,
                                     const ZfResolvedCapabilities *capabilities);
static void zf_transport_handle_cont_packet(ZerofidoApp *app, ZfTransportState *transport,
                                            const uint8_t *packet, size_t packet_len,
                                            uint32_t *actions);
static void zf_transport_refresh_lock(ZfTransportState *transport);
static bool zf_transport_lock_blocks_cid(ZfTransportState *transport, uint32_t cid);
static void zf_transport_add_action(uint32_t *actions, uint32_t action);

static bool zf_transport_cid_is_reserved(uint32_t cid) {
    return cid == ZF_RESERVED_CID || cid == ZF_BROADCAST_CID;
}

static bool zf_transport_cid_is_allocated(const ZfTransportState *transport, uint32_t cid) {
    if (zf_transport_cid_is_reserved(cid)) {
        return false;
    }

    for (size_t i = 0; i < transport->allocated_count; ++i) {
        if (transport->allocated_cids[i].cid == cid) {
            return true;
        }
    }

    return false;
}

static ptrdiff_t zf_transport_find_lru_reclaim_index(ZfTransportState *transport) {
    ptrdiff_t best_index = -1;
    uint32_t best_last_used = UINT32_MAX;

    zf_transport_refresh_lock(transport);
    for (size_t i = 0; i < transport->allocated_count; ++i) {
        uint32_t cid = transport->allocated_cids[i].cid;

        if (cid == transport->cid || cid == transport->lock_cid) {
            continue;
        }
        if (best_index < 0 || transport->allocated_cids[i].last_used < best_last_used) {
            best_index = (ptrdiff_t)i;
            best_last_used = transport->allocated_cids[i].last_used;
        }
    }

    return best_index;
}

static void zf_transport_touch_cid(ZfTransportState *transport, uint32_t cid) {
    uint32_t now = 0;

    if (zf_transport_cid_is_reserved(cid)) {
        return;
    }

    now = furi_get_tick();
    for (size_t i = 0; i < transport->allocated_count; ++i) {
        if (transport->allocated_cids[i].cid == cid) {
            transport->allocated_cids[i].last_used = now;
            return;
        }
    }
}

static bool zf_transport_remember_cid(ZfTransportState *transport, uint32_t cid) {
    uint32_t now = furi_get_tick();

    if (zf_transport_cid_is_reserved(cid)) {
        return false;
    }

    for (size_t i = 0; i < transport->allocated_count; ++i) {
        if (transport->allocated_cids[i].cid == cid) {
            transport->allocated_cids[i].last_used = now;
            return true;
        }
    }

    if (transport->allocated_count < ZF_MAX_ALLOCATED_CIDS) {
        transport->allocated_cids[transport->allocated_count].cid = cid;
        transport->allocated_cids[transport->allocated_count].last_used = now;
        transport->allocated_count++;
        return true;
    }

    ptrdiff_t reclaim_index = zf_transport_find_lru_reclaim_index(transport);
    if (reclaim_index >= 0) {
        transport->allocated_cids[reclaim_index].cid = cid;
        transport->allocated_cids[reclaim_index].last_used = now;
        return true;
    }

    return false;
}

/*
 * Allocates a random CTAPHID channel ID. Reserved CIDs are never assigned, and
 * when the table is full the least-recently-used non-active, non-locked CID is
 * reclaimed.
 */
static bool zf_transport_allocate_cid(ZfTransportState *transport, uint32_t *out_cid) {
    for (size_t attempt = 0; attempt < 32; ++attempt) {
        uint32_t cid = furi_hal_random_get();

        if (zf_transport_cid_is_reserved(cid) || zf_transport_cid_is_allocated(transport, cid)) {
            continue;
        }
        if (!zf_transport_remember_cid(transport, cid)) {
            return false;
        }

        *out_cid = cid;
        return true;
    }

    return false;
}

static void zf_transport_add_action(uint32_t *actions, uint32_t action) {
    if (actions) {
        *actions |= action;
    }
}

static bool zf_transport_try_send_u2f_validation_error(const ZerofidoApp *app,
                                                       ZfTransportState *transport) {
    ZfResolvedCapabilities capabilities;
    uint8_t status[2] = {0};
    uint16_t status_len = 0;

    if (!app || !transport || transport->cmd != ZF_CTAPHID_MSG) {
        return false;
    }

    zf_runtime_get_effective_capabilities(app, &capabilities);
    if (!capabilities.u2f_enabled) {
        return false;
    }

    status_len = u2f_validate_request_into_response(transport->payload, transport->total_len,
                                                    status, sizeof(status));
    if (status_len == 0) {
        return false;
    }

    zf_transport_session_send_frames(transport->cid, ZF_CTAPHID_MSG, status, status_len);
    zf_transport_session_reset(transport);
    return true;
}

void zf_transport_session_attach_arena(ZfTransportState *transport, uint8_t *payload,
                                       size_t payload_capacity) {
    if (!transport) {
        return;
    }

    transport->payload = payload;
    transport->payload_capacity = payload_capacity;
}

static bool zf_transport_ensure_payload(ZerofidoApp *app, ZfTransportState *transport) {
    if (!transport) {
        return false;
    }
    if (!transport->payload && app) {
        if (!zf_app_transport_arena_acquire(app)) {
            return false;
        }
        zf_transport_session_attach_arena(transport, app->transport_arena, ZF_MAX_MSG_SIZE);
    }
    return transport->payload && transport->payload_capacity >= ZF_MAX_MSG_SIZE;
}

static uint8_t zf_transport_resync_processing(const ZerofidoApp *app, ZfTransportState *transport,
                                              uint32_t cid, const uint8_t *packet,
                                              size_t packet_len, uint16_t msg_len,
                                              uint32_t *actions) {
    ZfResolvedCapabilities capabilities;

    if (msg_len != 8 || (packet_len - 7) < msg_len) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
        return ZF_CTAP_SUCCESS;
    }
    if (!zf_transport_cid_is_allocated(transport, cid)) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_CHANNEL);
        return ZF_CTAP_SUCCESS;
    }

    transport->processing_resync = true;
    transport->processing = false;
    transport->processing_generation++;
    zf_transport_add_action(actions, ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION);
    zf_transport_touch_cid(transport, cid);
    zf_runtime_get_effective_capabilities(app, &capabilities);
    zf_transport_handle_init(cid, cid, &packet[7], msg_len, &capabilities);
    return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
}

static uint8_t zf_transport_recover_processing_with_broadcast_init(
    const ZerofidoApp *app, ZfTransportState *transport, const uint8_t *packet, size_t packet_len,
    uint16_t msg_len, uint32_t *actions) {
    ZfResolvedCapabilities capabilities;
    uint32_t assigned_cid = 0;

    if (msg_len != 8 || (packet_len - 7) < msg_len) {
        zf_transport_session_send_error(ZF_BROADCAST_CID, ZF_HID_ERR_INVALID_LEN);
        return ZF_CTAP_SUCCESS;
    }
    if (!zf_transport_allocate_cid(transport, &assigned_cid)) {
        zf_transport_session_send_error(ZF_BROADCAST_CID, ZF_HID_ERR_OTHER);
        return ZF_CTAP_SUCCESS;
    }

    transport->processing_resync = true;
    transport->processing = false;
    transport->processing_cancel_requested = true;
    transport->processing_generation++;
    zf_transport_add_action(actions, ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION);
    zf_runtime_get_effective_capabilities(app, &capabilities);
    zf_transport_handle_init(ZF_BROADCAST_CID, assigned_cid, &packet[7], msg_len, &capabilities);
    return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
}

/*
 * Serializes one CTAPHID response: an initial frame carries CID, command, total
 * length, and up to 57 bytes; continuation frames carry CID, sequence number,
 * and up to 59 bytes. Every HID report is padded to 64 bytes by zero fill.
 */
void zf_transport_session_send_frames(uint32_t cid, uint8_t cmd, const uint8_t *data, size_t size) {
    uint8_t packet[ZF_CTAPHID_PACKET_SIZE];
    uint8_t seq = 0;
    size_t chunk = 0;
    size_t offset = 0;

    /*
     * U2F conformance meta tests can produce immediate two-byte MSG responses.
     * Give the Flipper U2F HAL a small turn-around window before queueing the IN report; otherwise
     * the HAL can silently miss the response under fast host OUT->IN traffic.
     */
    if (cmd == ZF_CTAPHID_MSG) {
        furi_delay_ms(ZF_U2F_HID_RESPONSE_SETTLE_MS);
    }

    memset(packet, 0, sizeof(packet));
    memcpy(packet, &cid, sizeof(cid));
    packet[4] = cmd;
    packet[5] = (uint8_t)(size >> 8);
    packet[6] = (uint8_t)size;

    chunk = size > (ZF_CTAPHID_PACKET_SIZE - 7) ? (ZF_CTAPHID_PACKET_SIZE - 7) : size;
    if (chunk > 0) {
        memcpy(&packet[7], data, chunk);
    }
    furi_hal_hid_u2f_send_response(packet, sizeof(packet));
    offset += chunk;

    while (offset < size) {
        memset(packet, 0, sizeof(packet));
        memcpy(packet, &cid, sizeof(cid));
        packet[4] = seq++;
        chunk = (size - offset) > (ZF_CTAPHID_PACKET_SIZE - 5) ? (ZF_CTAPHID_PACKET_SIZE - 5)
                                                               : (size - offset);
        memcpy(&packet[5], &data[offset], chunk);
        furi_hal_hid_u2f_send_response(packet, sizeof(packet));
        offset += chunk;
    }
}

void zf_transport_session_send_error(uint32_t cid, uint8_t hid_error) {
    zf_transport_session_send_frames(cid, ZF_CTAPHID_ERROR, &hid_error, 1);
}

static void zf_transport_send_lock_response(uint32_t cid) {
    zf_transport_session_send_frames(cid, ZF_CTAPHID_LOCK, NULL, 0);
}

static void zf_transport_refresh_lock(ZfTransportState *transport) {
    if (transport->lock_cid == 0) {
        return;
    }

    if ((int32_t)(furi_get_tick() - transport->lock_expires_at) >= 0) {
        transport->lock_cid = 0;
        transport->lock_expires_at = 0;
    }
}

static bool zf_transport_lock_blocks_cid(ZfTransportState *transport, uint32_t cid) {
    zf_transport_refresh_lock(transport);
    return transport->lock_cid != 0 && cid != transport->lock_cid;
}

static void zf_transport_handle_init(uint32_t response_cid, uint32_t assigned_cid,
                                     const uint8_t *payload, size_t payload_len,
                                     const ZfResolvedCapabilities *capabilities) {
    uint8_t response[17];
    uint8_t capability_flags = 0;

    memset(response, 0, sizeof(response));
    memcpy(response, payload, payload_len > 8 ? 8 : payload_len);
    memcpy(&response[8], &assigned_cid, sizeof(assigned_cid));
    response[12] = 2;
    response[13] = 1;
    response[14] = 4;
    response[15] = 3;
    if (capabilities && capabilities->transport_wink_enabled) {
        capability_flags |= ZF_CAPABILITY_WINK;
    }
    if (capabilities && capabilities->fido2_enabled) {
        capability_flags |= ZF_CAPABILITY_CBOR;
    }
    response[16] = capability_flags;
    zf_transport_session_send_frames(response_cid, ZF_CTAPHID_INIT, response, sizeof(response));
}

static bool zf_transport_is_busy_for_cid(const ZfTransportState *transport, uint32_t cid) {
    return transport->active && cid != transport->cid;
}

static void zf_transport_send_busy_or_invalid_len(const ZfTransportState *transport, uint32_t cid) {
    if (cid != transport->cid) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
    } else {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
    }
}

static void zf_transport_send_busy_or_invalid_seq(const ZfTransportState *transport, uint32_t cid) {
    if (cid != transport->cid) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
    } else {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_SEQ);
    }
}

void zf_transport_session_reset(ZfTransportState *transport) {
    size_t used = transport->total_len > transport->received_len ? transport->total_len
                                                                 : transport->received_len;

    if (used > transport->payload_capacity) {
        used = transport->payload_capacity;
    }
    if (transport->payload && used > 0U) {
        zf_crypto_secure_zero(transport->payload, used);
    }
    transport->active = false;
    transport->processing = false;
    transport->processing_resync = false;
    transport->processing_cancel_requested = false;
    transport->stopping = false;
    transport->processing_generation = 0;
    transport->cid = 0;
    transport->cmd = 0;
    transport->total_len = 0;
    transport->received_len = 0;
    transport->next_seq = 0;
    transport->last_activity = 0;
}

/*
 * While a CTAP2 request is processing, only control packets that can unblock or
 * resync the channel are honored: CANCEL, same-CID INIT, and broadcast INIT.
 * Other CIDs receive busy responses so the worker keeps one in-flight request.
 */
uint8_t zf_transport_session_handle_processing_control(const ZerofidoApp *app,
                                                       ZfTransportState *transport,
                                                       const uint8_t *packet, size_t packet_len,
                                                       uint32_t *actions) {
    uint8_t cmd = 0;
    uint16_t msg_len = 0;
    uint32_t cid = 0;

    if (packet_len < 5) {
        return ZF_CTAP_SUCCESS;
    }

    memcpy(&cid, packet, sizeof(cid));
    if ((packet[4] & ZF_CTAPHID_TYPE_INIT) == 0) {
        return ZF_CTAP_SUCCESS;
    }

    if (packet_len < 7) {
        zf_transport_send_busy_or_invalid_len(transport, cid);
        return ZF_CTAP_SUCCESS;
    }

    cmd = packet[4];
    msg_len = ((uint16_t)packet[5] << 8) | packet[6];
    if (cmd == ZF_CTAPHID_CANCEL) {
        if (msg_len != 0) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
            return ZF_CTAP_SUCCESS;
        }
        if (transport->processing && transport->cmd == ZF_CTAPHID_CBOR && cid == transport->cid) {
            transport->processing_cancel_requested = true;
            zf_transport_add_action(actions, ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION);
            return ZF_CTAP_ERR_KEEPALIVE_CANCEL;
        }
        return ZF_CTAP_SUCCESS;
    }

    if (cmd == ZF_CTAPHID_INIT && cid == transport->cid) {
        return zf_transport_resync_processing(app, transport, cid, packet, packet_len, msg_len,
                                              actions);
    }
    if (cmd == ZF_CTAPHID_INIT && cid == ZF_BROADCAST_CID) {
        return zf_transport_recover_processing_with_broadcast_init(app, transport, packet,
                                                                   packet_len, msg_len, actions);
    }

    if (cid != transport->cid) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
    }
    return ZF_CTAP_SUCCESS;
}

static void zf_transport_complete_if_ready(ZerofidoApp *app, ZfTransportState *transport) {
    ZfTransportProtocolKind protocol = ZfTransportProtocolKindPing;

    if (transport->received_len != transport->total_len) {
        return;
    }

    transport->active = false;
    if (zf_transport_try_send_u2f_validation_error(app, transport)) {
        return;
    }

    switch (transport->cmd) {
    case ZF_CTAPHID_PING:
        protocol = ZfTransportProtocolKindPing;
        break;
    case ZF_CTAPHID_MSG:
        protocol = ZfTransportProtocolKindU2f;
        break;
    case ZF_CTAPHID_WINK:
        protocol = ZfTransportProtocolKindWink;
        break;
    case ZF_CTAPHID_CBOR:
        protocol = ZfTransportProtocolKindCtap2;
        break;
    default:
        zf_transport_session_send_error(transport->cid, ZF_HID_ERR_INVALID_CMD);
        return;
    }

    if (!zf_transport_ensure_payload(app, transport)) {
        zf_transport_session_send_error(transport->cid, ZF_HID_ERR_OTHER);
        return;
    }

    zf_transport_dispatch_complete_message(app, transport, transport->cid, protocol,
                                           transport->payload, transport->total_len);
}

static bool zf_transport_validate_command_length(uint32_t cid, uint8_t cmd, uint16_t msg_len) {
    bool valid = true;

    switch (cmd) {
    case ZF_CTAPHID_MSG:
    case ZF_CTAPHID_CBOR:
        valid = msg_len > 0;
        break;
    case ZF_CTAPHID_LOCK:
        valid = msg_len == 1;
        break;
    case ZF_CTAPHID_WINK:
        valid = msg_len == 0;
        break;
    default:
        valid = true;
        break;
    }

    if (!valid) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
    }
    return valid;
}

static bool zf_transport_begin_message(ZfTransportState *transport, uint32_t cid, uint8_t cmd,
                                       uint16_t msg_len) {
    if (!transport->payload || msg_len > transport->payload_capacity || msg_len > ZF_MAX_MSG_SIZE) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
        zf_transport_session_reset(transport);
        return false;
    }
    if (!zf_transport_validate_command_length(cid, cmd, msg_len)) {
        zf_transport_session_reset(transport);
        return false;
    }

    zf_transport_session_reset(transport);
    transport->active = true;
    transport->cid = cid;
    transport->cmd = cmd;
    transport->total_len = msg_len;
    transport->next_seq = 0;
    transport->last_activity = furi_get_tick();
    return true;
}

static void zf_transport_copy_init_payload(ZfTransportState *transport, const uint8_t *packet,
                                           size_t packet_len) {
    size_t chunk = packet_len - 7;

    if (chunk > transport->total_len) {
        chunk = transport->total_len;
    }
    if (chunk == 0) {
        return;
    }

    memcpy(transport->payload, &packet[7], chunk);
    transport->received_len = chunk;
}

/*
 * Handles initial CTAPHID command frames before payload assembly. This is where
 * broadcast INIT channel allocation, LOCK ownership, command-specific length
 * checks, and invalid-channel/busy responses are enforced.
 */
static bool zf_transport_handle_init_command(const ZerofidoApp *app, ZfTransportState *transport,
                                             uint32_t cid, uint8_t cmd, uint16_t msg_len,
                                             const uint8_t *packet, size_t packet_len,
                                             uint32_t *actions) {
    ZfResolvedCapabilities capabilities;

    uint32_t assigned_cid = cid;
    zf_runtime_get_effective_capabilities(app, &capabilities);

    if (cmd == ZF_CTAPHID_CANCEL) {
        if (msg_len != 0) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
            return true;
        }
        if (transport->processing && transport->cmd == ZF_CTAPHID_CBOR && cid == transport->cid) {
            transport->processing_cancel_requested = true;
            zf_transport_add_action(actions, ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION);
        }
        return true;
    }
    if (cmd == ZF_CTAPHID_LOCK) {
        uint8_t lock_seconds = 0;

        if (msg_len != 1 || (packet_len - 7) < msg_len) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
            return true;
        }
        if (!zf_transport_cid_is_allocated(transport, cid)) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_CHANNEL);
            return true;
        }

        lock_seconds = packet[7];
        if (lock_seconds > 10) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_PAR);
            return true;
        }

        zf_transport_touch_cid(transport, cid);
        if (lock_seconds == 0) {
            if (transport->lock_cid == cid) {
                transport->lock_cid = 0;
                transport->lock_expires_at = 0;
            }
        } else {
            transport->lock_cid = cid;
            transport->lock_expires_at = furi_get_tick() + ((uint32_t)lock_seconds * 1000U);
        }
        zf_transport_send_lock_response(cid);
        return true;
    }
    if (cmd == ZF_CTAPHID_INIT) {
        if (msg_len != 8) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
            return true;
        }
        if ((packet_len - 7) < msg_len) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
            return true;
        }
        if (transport->active && cid != transport->cid) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
            return true;
        }
        if (cid == ZF_BROADCAST_CID) {
            if (!zf_transport_allocate_cid(transport, &assigned_cid)) {
                zf_transport_session_send_error(cid, ZF_HID_ERR_OTHER);
                return true;
            }
        } else if (!zf_transport_cid_is_allocated(transport, cid)) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_CHANNEL);
            return true;
        } else {
            zf_transport_touch_cid(transport, cid);
        }

        zf_transport_session_reset(transport);
        zf_transport_handle_init(cid, assigned_cid, &packet[7], msg_len, &capabilities);
        return true;
    }
    if (transport->active) {
        zf_transport_send_busy_or_invalid_seq(transport, cid);
        if (cid == transport->cid) {
            zf_transport_session_reset(transport);
        }
        return true;
    }
    return false;
}

static void zf_transport_handle_init_packet(ZerofidoApp *app, ZfTransportState *transport,
                                            const uint8_t *packet, size_t packet_len,
                                            uint32_t *actions) {
    uint8_t cmd = 0;
    uint16_t msg_len = 0;
    uint32_t cid = 0;

    memcpy(&cid, packet, sizeof(cid));
    if (packet_len < 7) {
        if (packet_len >= 5 && zf_transport_is_busy_for_cid(transport, cid)) {
            zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
        } else {
            zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_LEN);
        }
        return;
    }

    cmd = packet[4];
    if (cmd != ZF_CTAPHID_CANCEL && zf_transport_is_busy_for_cid(transport, cid)) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
        return;
    }

    msg_len = ((uint16_t)packet[5] << 8) | packet[6];
    if (zf_transport_handle_init_command(app, transport, cid, cmd, msg_len, packet, packet_len,
                                         actions)) {
        return;
    }
    if (!zf_transport_cid_is_allocated(transport, cid)) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_CHANNEL);
        return;
    }
    zf_transport_touch_cid(transport, cid);
    if (!zf_transport_ensure_payload(app, transport)) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_OTHER);
        return;
    }
    if (!zf_transport_begin_message(transport, cid, cmd, msg_len)) {
        return;
    }

    zf_transport_copy_init_payload(transport, packet, packet_len);
    zf_transport_complete_if_ready(app, transport);
}

static void zf_transport_handle_processing_packet(ZerofidoApp *app, ZfTransportState *transport,
                                                  const uint8_t *packet, size_t packet_len,
                                                  uint32_t *actions) {
    uint32_t cid = 0;

    memcpy(&cid, packet, sizeof(cid));
    if (packet_len < 7) {
        zf_transport_send_busy_or_invalid_len(transport, cid);
        return;
    }

    if ((packet[4] & ZF_CTAPHID_TYPE_INIT) == 0) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_SEQ);
        return;
    }

    if (packet[4] == ZF_CTAPHID_CANCEL) {
        zf_transport_handle_init_packet(app, transport, packet, packet_len, actions);
        return;
    }

    if (packet[4] == ZF_CTAPHID_INIT && cid == transport->cid) {
        uint16_t msg_len = ((uint16_t)packet[5] << 8) | packet[6];

        zf_transport_resync_processing(app, transport, cid, packet, packet_len, msg_len, actions);
        return;
    }
    if (packet[4] == ZF_CTAPHID_INIT && cid == ZF_BROADCAST_CID) {
        uint16_t msg_len = ((uint16_t)packet[5] << 8) | packet[6];

        zf_transport_recover_processing_with_broadcast_init(app, transport, packet, packet_len,
                                                            msg_len, actions);
        return;
    }

    zf_transport_send_busy_or_invalid_seq(transport, cid);
}

static void zf_transport_handle_idle_packet(ZerofidoApp *app, ZfTransportState *transport,
                                            const uint8_t *packet, size_t packet_len,
                                            uint32_t *actions) {
    if (packet[4] & ZF_CTAPHID_TYPE_INIT) {
        zf_transport_handle_init_packet(app, transport, packet, packet_len, actions);
        return;
    }

    zf_transport_handle_cont_packet(app, transport, packet, packet_len, actions);
}

static bool zf_transport_validate_continuation(const ZfTransportState *transport, uint32_t cid,
                                               uint8_t seq) {
    if (!transport->active) {
        return false;
    }
    if (seq != transport->next_seq || cid != transport->cid) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_INVALID_SEQ);
        return false;
    }
    return true;
}

static void zf_transport_copy_continuation_payload(ZfTransportState *transport,
                                                   const uint8_t *packet, size_t packet_len) {
    size_t remaining = transport->total_len - transport->received_len;
    size_t chunk = packet_len - 5;

    if (chunk > remaining) {
        chunk = remaining;
    }
    if (chunk == 0) {
        return;
    }

    memcpy(&transport->payload[transport->received_len], &packet[5], chunk);
    transport->received_len += chunk;
}

static void zf_transport_handle_cont_packet(ZerofidoApp *app, ZfTransportState *transport,
                                            const uint8_t *packet, size_t packet_len,
                                            uint32_t *actions) {
    uint32_t cid = 0;
    uint8_t seq = 0;
    bool invalid_seq = false;

    UNUSED(actions);

    memcpy(&cid, packet, sizeof(cid));
    seq = packet[4];
    invalid_seq = transport->active && (seq != transport->next_seq || cid != transport->cid);
    if (!zf_transport_validate_continuation(transport, cid, seq)) {
        if (invalid_seq) {
            zf_transport_session_reset(transport);
        }
        return;
    }

    zf_transport_touch_cid(transport, cid);
    transport->next_seq++;
    transport->last_activity = furi_get_tick();
    zf_transport_copy_continuation_payload(transport, packet, packet_len);
    zf_transport_complete_if_ready(app, transport);
}

void zf_transport_session_handle_packet(ZerofidoApp *app, ZfTransportState *transport,
                                        const uint8_t *packet, size_t packet_len,
                                        uint32_t *actions) {
    uint32_t cid = 0;

    if (packet_len < 5) {
        return;
    }

    memcpy(&cid, packet, sizeof(cid));
    if (zf_transport_lock_blocks_cid(transport, cid)) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
        return;
    }
    if (transport->processing) {
        zf_transport_handle_processing_packet(app, transport, packet, packet_len, actions);
        return;
    }
    if (!transport->active && zf_transport_is_busy_for_cid(transport, cid)) {
        zf_transport_session_send_error(cid, ZF_HID_ERR_CHANNEL_BUSY);
        return;
    }

    zf_transport_handle_idle_packet(app, transport, packet, packet_len, actions);
}

void zf_transport_session_expire_lock(ZfTransportState *transport) {
    transport->lock_cid = 0;
    transport->lock_expires_at = 0;
}

void zf_transport_session_tick(ZfTransportState *transport, uint32_t now) {
    if (transport->active && (uint32_t)(now - transport->last_activity) >= ZF_ASSEMBLY_TIMEOUT_MS) {
        zf_transport_session_send_error(transport->cid, ZF_HID_ERR_MSG_TIMEOUT);
        zf_transport_session_reset(transport);
    }

    if (transport->lock_cid != 0 && transport->lock_expires_at != 0 &&
        (int32_t)(now - transport->lock_expires_at) >= 0) {
        zf_transport_session_expire_lock(transport);
    }
}

#endif
