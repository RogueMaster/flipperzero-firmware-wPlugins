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

#include "../../zerofido_pin.h"
#include "../../zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

/*
 * Shared CTAP command utilities: scratch allocation, maintenance locking, PIN
 * auth enforcement, and idle checks used by several command handlers.
 */
void *zf_ctap_command_scratch(ZerofidoApp *app, size_t size);
bool zf_ctap_begin_maintenance(ZerofidoApp *app);
void zf_ctap_end_maintenance(ZerofidoApp *app);
uint8_t zf_ctap_require_pin_auth_with_state(ZerofidoApp *app, ZfClientPinState *pin_state,
                                            bool uv_requested, bool has_pin_auth,
                                            const uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN],
                                            const uint8_t *pin_auth, size_t pin_auth_len,
                                            bool has_pin_protocol, uint64_t pin_protocol,
                                            const char *rp_id, uint64_t required_permissions,
                                            bool *uv_verified);
uint8_t zf_ctap_dispatch_require_idle(ZerofidoApp *app);
