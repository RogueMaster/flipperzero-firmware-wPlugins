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

#include "../zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

#define ZF_CTAPHID_TYPE_INIT 0x80
#define ZF_CTAPHID_PING 0x81
#define ZF_CTAPHID_MSG 0x83
#define ZF_CTAPHID_LOCK 0x84
#define ZF_CTAPHID_INIT 0x86
#define ZF_CTAPHID_WINK 0x88
#define ZF_CTAPHID_CBOR 0x90
#define ZF_CTAPHID_CANCEL 0x91
#define ZF_CTAPHID_KEEPALIVE 0xBB
#define ZF_CTAPHID_ERROR 0xBF

#define ZF_HID_ERR_INVALID_CMD 0x01
#define ZF_HID_ERR_INVALID_PAR 0x02
#define ZF_HID_ERR_INVALID_LEN 0x03
#define ZF_HID_ERR_INVALID_SEQ 0x04
#define ZF_HID_ERR_MSG_TIMEOUT 0x05
#define ZF_HID_ERR_CHANNEL_BUSY 0x06
#define ZF_HID_ERR_INVALID_CHANNEL 0x0B
#define ZF_HID_ERR_OTHER 0x7F

#define ZF_CAPABILITY_WINK 0x01
#define ZF_CAPABILITY_CBOR 0x04
#define ZF_BROADCAST_CID 0xFFFFFFFFUL
#define ZF_RESERVED_CID 0x00000000UL
#define ZF_MAX_ALLOCATED_CIDS 32

enum {
    ZF_TRANSPORT_ACTION_NONE = 0,
    ZF_TRANSPORT_ACTION_CANCEL_PENDING_INTERACTION = (1U << 0),
};

/*
 * CTAPHID session state assembles 64-byte packets into one protocol message,
 * tracks allocated channels and locks, and exposes control-packet handling for
 * long-running CBOR commands.
 */
typedef struct {
    uint32_t cid;
    uint32_t last_used;
} ZfAllocatedCid;

struct ZfTransportState {
    bool active;
    bool processing;
    bool processing_resync;
    bool processing_cancel_requested;
    bool stopping;
    uint32_t processing_generation;
    uint32_t cid;
    uint8_t cmd;
    uint16_t total_len;
    uint16_t received_len;
    uint8_t next_seq;
    uint32_t last_activity;
    uint32_t lock_cid;
    uint32_t lock_expires_at;
    uint8_t *payload;
    size_t payload_capacity;
    ZfAllocatedCid allocated_cids[ZF_MAX_ALLOCATED_CIDS];
    size_t allocated_count;
};

typedef struct ZfTransportState ZfTransportState;

void zf_transport_session_attach_arena(ZfTransportState *transport, uint8_t *payload,
                                       size_t payload_capacity);
void zf_transport_session_reset(ZfTransportState *transport);
void zf_transport_session_send_frames(uint32_t cid, uint8_t cmd, const uint8_t *data, size_t size);
void zf_transport_session_send_error(uint32_t cid, uint8_t hid_error);
uint8_t zf_transport_session_handle_processing_control(const ZerofidoApp *app,
                                                       ZfTransportState *transport,
                                                       const uint8_t *packet, size_t packet_len,
                                                       uint32_t *actions);
void zf_transport_session_handle_packet(ZerofidoApp *app, ZfTransportState *transport,
                                        const uint8_t *packet, size_t packet_len,
                                        uint32_t *actions);
void zf_transport_session_tick(ZfTransportState *transport, uint32_t now);
void zf_transport_session_expire_lock(ZfTransportState *transport);
