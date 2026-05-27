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

#include <stddef.h>
#include <stdint.h>

#include "zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

/*
 * Transport-facing CTAP2 entry point. It consumes the command byte plus payload
 * and writes the CTAP status byte followed by any CBOR response.
 */
size_t zerofido_handle_ctap2(ZerofidoApp *app, ZfTransportSessionId session_id,
                             const uint8_t *request, size_t request_len, uint8_t *response,
                             size_t response_capacity);
void zerofido_ctap_invalidate_assertion_queue(ZerofidoApp *app);
