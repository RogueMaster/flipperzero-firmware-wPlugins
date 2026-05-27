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

#include "cred_protect.h"

#include "../../zerofido_types.h"

uint8_t zf_ctap_cred_protect_effective(uint8_t cred_protect) {
    return cred_protect == 0U ? ZF_CRED_PROTECT_UV_OPTIONAL : cred_protect;
}

bool zf_ctap_cred_protect_value_is_valid(uint64_t cred_protect) {
    return cred_protect >= ZF_CRED_PROTECT_UV_OPTIONAL &&
           cred_protect <= ZF_CRED_PROTECT_UV_REQUIRED;
}

bool zf_ctap_cred_protect_allows_assertion(uint8_t cred_protect, bool uv_verified,
                                           bool uses_allow_list) {
    switch (zf_ctap_cred_protect_effective(cred_protect)) {
    case ZF_CRED_PROTECT_UV_OPTIONAL:
        return true;
    case ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID:
        return uv_verified || uses_allow_list;
    case ZF_CRED_PROTECT_UV_REQUIRED:
        return uv_verified;
    default:
        return false;
    }
}

bool zf_ctap_cred_protect_encode_make_credential_output(ZfCborEncoder *enc,
                                                        uint8_t cred_protect) {
    return zf_cbor_encode_text(enc, "credProtect") &&
           zf_cbor_encode_uint(enc, zf_ctap_cred_protect_effective(cred_protect));
}
