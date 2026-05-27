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

#include "../../zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

/*
 * Approval helpers translate CTAP requirements into UI interactions and return
 * CTAP status codes. Empty pinAuth probes deliberately ask for touch before
 * reporting the pinAuth-specific failure required by conformance tests.
 */
uint8_t zf_ctap_request_approval(ZerofidoApp *app, const char *operation, const char *rp_id,
                                 const char *user_text, ZfTransportSessionId session_id);
uint8_t zf_ctap_request_assertion_selection(ZerofidoApp *app, const char *rp_id,
                                            const uint16_t *match_indices, size_t match_count,
                                            ZfTransportSessionId session_id,
                                            uint32_t *selected_record_index);
uint8_t zf_ctap_handle_empty_pin_auth_probe(ZerofidoApp *app, ZfTransportSessionId session_id,
                                            const char *operation, const char *rp_id,
                                            const char *user_text);
