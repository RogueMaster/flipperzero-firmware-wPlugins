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
typedef struct ZfTransportState ZfTransportState;

typedef enum {
    ZfTransportProtocolKindPing = 0,
    ZfTransportProtocolKindU2f = 1,
    ZfTransportProtocolKindCtap2 = 2,
    ZfTransportProtocolKindWink = 3,
} ZfTransportProtocolKind;

typedef struct {
    ZfTransportSessionId session_id;
    ZfTransportProtocolKind protocol;
    const uint8_t *payload;
    size_t payload_len;
} ZfProtocolDispatchRequest;

typedef struct {
    uint8_t *response;
    size_t response_capacity;
    size_t response_len;
    bool send_transport_error;
    uint8_t transport_error;
} ZfProtocolDispatchResult;

/*
 * Common protocol handoff used by USB HID and NFC. Transport code identifies
 * the protocol kind and arena; this function runs CTAP/U2F/WINK handling and
 * returns a transport-neutral dispatch result.
 */
void zf_transport_dispatch_complete_message(ZerofidoApp *app, ZfTransportState *transport,
                                            ZfTransportSessionId session_id,
                                            ZfTransportProtocolKind protocol,
                                            const uint8_t *payload, size_t payload_len);
