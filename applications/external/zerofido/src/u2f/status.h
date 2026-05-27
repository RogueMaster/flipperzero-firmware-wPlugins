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

#define ZF_U2F_STATUS_SIZE 2U

#define ZF_U2F_SW_NO_ERROR 0x9000U
#define ZF_U2F_SW_WRONG_LENGTH 0x6700U
#define ZF_U2F_SW_CONDITIONS_NOT_SATISFIED 0x6985U
#define ZF_U2F_SW_WRONG_DATA 0x6A80U
#define ZF_U2F_SW_INS_NOT_SUPPORTED 0x6D00U
#define ZF_U2F_SW_CLA_NOT_SUPPORTED 0x6E00U

static inline uint16_t zf_u2f_write_status(uint8_t *buf, uint16_t status) {
    buf[0] = (uint8_t)(status >> 8);
    buf[1] = (uint8_t)status;
    return ZF_U2F_STATUS_SIZE;
}
