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

#include "apdu.h"

#include "apdu_internal.h"
#include "status.h"

static uint16_t u2f_validate_apdu_status(const uint8_t *buf, uint16_t len, uint8_t ins,
                                         uint32_t expected_lc, bool allow_short) {
    U2fParsedApdu apdu = {0};

    if (!u2f_parse_apdu_header(buf, len, allow_short, &apdu)) {
        return ZF_U2F_SW_WRONG_LENGTH;
    }
    if (apdu.cla != 0x00) {
        return ZF_U2F_SW_CLA_NOT_SUPPORTED;
    }
    if (apdu.ins != ins) {
        return ZF_U2F_SW_INS_NOT_SUPPORTED;
    }
    if (apdu.lc != expected_lc) {
        return ZF_U2F_SW_WRONG_LENGTH;
    }

    return 0;
}

static uint16_t u2f_validate_request_status(const uint8_t *buf, uint16_t request_len) {
    if (request_len < 4) {
        return ZF_U2F_SW_WRONG_LENGTH;
    }
    if (buf[0] != 0x00) {
        return ZF_U2F_SW_CLA_NOT_SUPPORTED;
    }
    if (buf[1] == U2F_CMD_VERSION) {
        if (request_len == 4) {
            return 0;
        }
        return u2f_validate_apdu_status(buf, request_len, U2F_CMD_VERSION, 0, true);
    }
    if (request_len < 5) {
        return ZF_U2F_SW_WRONG_LENGTH;
    }
    if (request_len < 7) {
        return ZF_U2F_SW_WRONG_LENGTH;
    }
    if (buf[1] == U2F_CMD_REGISTER) {
        return u2f_validate_apdu_status(buf, request_len, U2F_CMD_REGISTER,
                                        U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE, false);
    }
    if (buf[1] == U2F_CMD_AUTHENTICATE) {
        U2fParsedApdu apdu = {0};
        if (!u2f_parse_apdu_header(buf, request_len, false, &apdu)) {
            return ZF_U2F_SW_WRONG_LENGTH;
        }
        if (apdu.p1 != U2fCheckOnly && apdu.p1 != U2fEnforce && apdu.p1 != U2fDontEnforce) {
            return ZF_U2F_SW_WRONG_DATA;
        }
        if (apdu.lc < (U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1)) {
            return ZF_U2F_SW_WRONG_LENGTH;
        }
        if (apdu.data == NULL) {
            return ZF_U2F_SW_WRONG_LENGTH;
        }
        uint8_t key_handle_len = apdu.data[U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE];
        if (apdu.lc != (uint32_t)(U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + key_handle_len)) {
            return ZF_U2F_SW_WRONG_LENGTH;
        }
        return 0;
    }
    return ZF_U2F_SW_INS_NOT_SUPPORTED;
}

/*
 * Parses the U2F APDU envelope forms accepted by the transport. Extended APDUs
 * carry the command payload; short VERSION-style APDUs are accepted only when
 * allow_short is set. Le is parsed for validation but ignored by U2F command
 * handling.
 */
bool u2f_parse_apdu_header(const uint8_t *buf, uint16_t len, bool allow_short,
                           U2fParsedApdu *apdu) {
    if (len < 4) {
        return false;
    }

    apdu->cla = buf[0];
    apdu->ins = buf[1];
    apdu->p1 = buf[2];
    apdu->p2 = buf[3];
    apdu->data = NULL;
    apdu->lc = 0;

    if (allow_short && len == 5) {
        return true;
    }

    if (len >= 7 && buf[4] == 0x00) {
        uint32_t lc = ((uint32_t)buf[5] << 8) | buf[6];
        if ((size_t)len != 7U + lc && (size_t)len != 9U + lc) {
            return false;
        }
        apdu->data = &buf[7];
        apdu->lc = lc;
        return true;
    }

    {
        uint32_t lc = buf[4];
        if ((size_t)len != 5U + lc && (size_t)len != 6U + lc) {
            return false;
        }
        apdu->data = &buf[5];
        apdu->lc = lc;
        return true;
    }
}

uint16_t u2f_validate_request(uint8_t *buf, uint16_t request_len) {
    uint16_t status = u2f_validate_request_status(buf, request_len);

    if (status != 0) {
        return zf_u2f_write_status(buf, status);
    }

    return 0;
}

uint16_t u2f_validate_request_into_response(const uint8_t *request, uint16_t request_len,
                                            uint8_t *response, uint16_t response_capacity) {
    uint16_t status = 0;

    if (!request || !response || response_capacity < 2) {
        return 0;
    }

    status = u2f_validate_request_status(request, request_len);
    if (status != 0) {
        return zf_u2f_write_status(response, status);
    }

    return 0;
}

bool u2f_request_needs_user_presence(const uint8_t *buf, uint16_t request_len,
                                     const char **operation) {
    if (!buf || request_len < 2 || !operation) {
        return false;
    }

    switch (buf[1]) {
    case U2F_CMD_REGISTER:
        *operation = "Register";
        return true;
    case U2F_CMD_AUTHENTICATE:
        if (request_len < 3) {
            return false;
        }
        if (buf[2] != U2fEnforce) {
            return false;
        }
        *operation = "Authenticate";
        return true;
    default:
        return false;
    }
}
