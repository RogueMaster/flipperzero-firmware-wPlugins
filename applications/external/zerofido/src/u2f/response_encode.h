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

#include <stdint.h>

#include "session.h"

/*
 * Encoders write complete U2F APDU responses, including status words. The
 * register path binds appId/challenge to an attestation signature; authenticate
 * binds the counter and user-presence byte to the credential key.
 */
uint16_t zf_u2f_encode_register_response(U2fData *instance, uint8_t *buf, uint16_t request_len,
                                         uint16_t response_capacity);
uint16_t zf_u2f_encode_authenticate_response(U2fData *instance, uint8_t *buf, uint16_t request_len,
                                             uint16_t response_capacity);
