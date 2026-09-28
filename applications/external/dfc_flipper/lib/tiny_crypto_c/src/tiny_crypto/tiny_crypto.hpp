/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_HPP_
#define TINY_CRYPTO_HPP_

#ifndef __cplusplus
#error Do not include tiny_crypto.hpp in a C project, include tiny_crypto.h instead
#endif

#include <tiny_crypto/tiny_crypto.h>
#include <tiny_crypto/common.hpp>
#if TC_ENABLE_GZIP
#include <tiny_crypto/gzip.hpp>
#endif
#if TC_ENABLE_RSA
#include <tiny_crypto/rsa.hpp>
#endif
#if TC_ENABLE_PIV_SM
#include <tiny_crypto/piv_sm.hpp>
#endif
#if TC_ENABLE_EC
#include <tiny_crypto/ec.hpp>
#endif

#if TC_ENABLE_TLV
#include <tiny_crypto/tlv.hpp>
#endif

#if TC_ENABLE_KMAC256
#include <tiny_crypto/kmac.hpp>
#endif

#if TC_ENABLE_AES
#include <tiny_crypto/aes.hpp>
#if TC_AES_ENABLE_DYNAMIC
#include <tiny_crypto/aes_dynamic.hpp>
#endif
#endif

#if TC_ENABLE_DES
#include <tiny_crypto/des.hpp>
#endif

#if TC_ENABLE_MD5 || TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 || \
    TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#include <tiny_crypto/hash.hpp>
#endif

#if TC_ENABLE_KDF
#include <tiny_crypto/kdf.hpp>
#endif

#if TC_ENABLE_SSKDF
#include <tiny_crypto/sskdf.hpp>
#endif

#endif
