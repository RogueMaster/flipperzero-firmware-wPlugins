/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_EC_HPP_
#define TINY_CRYPTO_EC_HPP_
#include <tiny_crypto/common.hpp>
#include <tiny_crypto/ec.h>

namespace tiny_crypto {
typedef ::TC_EC_curve ec_curve;
typedef ::TC_EC_workspace ec_workspace;
typedef ::TC_ECDSA_workspace ecdsa_workspace;

inline TC_status ec_public_key(ec_curve curve, bytes scalar, uint8_t* output,
                                size_t length, ec_workspace& workspace) noexcept {
    return ::TC_EC_public_key(curve, scalar.data, scalar.length, output, length, &workspace);
}
inline TC_status ec_validate_public_key(ec_curve curve, bytes key,
                                         ec_workspace& workspace) noexcept {
    return ::TC_EC_validate_public_key(curve, key.data, key.length, &workspace);
}
inline TC_status ecdh(ec_curve curve, bytes scalar, bytes peer, uint8_t* output,
                       size_t length, ec_workspace& workspace) noexcept {
    return ::TC_ECDH(curve, scalar.data, scalar.length, peer.data, peer.length,
                     output, length, &workspace);
}
inline TC_status ecdsa_verify_digest(ec_curve curve, bytes key, bytes digest,
                                     bytes signature, ecdsa_workspace& workspace) noexcept {
    return ::TC_ECDSA_verify_digest(curve, key.data, key.length, digest.data,
        digest.length, signature.data, signature.length, &workspace);
}
} // namespace tiny_crypto
#endif
