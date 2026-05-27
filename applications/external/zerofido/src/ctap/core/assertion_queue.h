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

#include "../../zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

/*
 * Stores remaining getAssertion matches after the first response. The queue is
 * bound to the transport session and expires after the CTAP assertion window.
 */
void zf_ctap_assertion_queue_clear(ZerofidoApp *app);
void zf_ctap_assertion_queue_seed(ZerofidoApp *app, ZfTransportSessionId session_id,
                                  const ZfGetAssertionRequest *request, bool uv_verified,
                                  const uint16_t *match_indices, size_t match_count);
uint8_t zf_ctap_assertion_queue_handle_next(ZerofidoApp *app, ZfTransportSessionId session_id,
                                            uint8_t *out, size_t out_capacity, size_t *out_len);
