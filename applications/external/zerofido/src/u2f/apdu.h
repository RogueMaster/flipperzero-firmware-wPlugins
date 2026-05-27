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
#include <stdint.h>

#include "common.h"

/*
 * APDU validation is shared by USB HID MSG and NFC U2F paths. It checks the
 * fixed U2F header shape and reports ISO7816 status words in response buffers.
 */
uint16_t u2f_validate_request(uint8_t *buf, uint16_t request_len);
uint16_t u2f_validate_request_into_response(const uint8_t *request, uint16_t request_len,
                                            uint8_t *response, uint16_t response_capacity);
bool u2f_request_needs_user_presence(const uint8_t *buf, uint16_t request_len,
                                     const char **operation);
