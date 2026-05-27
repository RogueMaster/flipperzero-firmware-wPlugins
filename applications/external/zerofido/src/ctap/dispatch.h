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

#include "../zerofido_runtime_config.h"
#include "../zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

/* Dispatches a parsed CTAP command byte to the enabled command implementation. */
uint8_t zf_ctap_dispatch_command(ZerofidoApp *app, const ZfResolvedCapabilities *capabilities,
                                 ZfTransportSessionId session_id, uint8_t cmd,
                                 const uint8_t *request_body, size_t request_body_len,
                                 uint8_t *response_body, size_t response_body_capacity,
                                 size_t *response_body_len);
