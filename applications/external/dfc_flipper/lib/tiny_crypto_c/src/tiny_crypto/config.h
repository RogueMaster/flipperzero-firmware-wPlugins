/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_CONFIG_H_
#define TINY_CRYPTO_CONFIG_H_
#include <tiny_crypto/resource_profile.h>

/* Algorithm selection. Disabled translation units can be omitted entirely by
 * CMake, while these values also gate the umbrella headers. */
#ifndef TC_ENABLE_AES
#define TC_ENABLE_AES TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_ENABLE_DES
#define TC_ENABLE_DES TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_SHA1
#define TC_ENABLE_SHA1 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_SHA224
#define TC_ENABLE_SHA224 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_SHA256
#define TC_ENABLE_SHA256 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_ENABLE_SHA384
#define TC_ENABLE_SHA384 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_SHA512
#define TC_ENABLE_SHA512 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_HMAC
#define TC_ENABLE_HMAC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_KMAC256
#define TC_ENABLE_KMAC256 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
/* TLV framing is independent of the cryptographic algorithms. DER adds typed
 * value checks; BER and incremental entry points are optional. Parser bounds
 * checks cannot be disabled with TC_STRICT. */
#ifndef TC_ENABLE_TLV
#define TC_ENABLE_TLV TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_DER
#define TC_ENABLE_DER TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_EAC_CVC
#define TC_ENABLE_EAC_CVC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_EAC_CVC != 0 && TC_ENABLE_EAC_CVC != 1
#error "TC_ENABLE_EAC_CVC must be 0 or 1"
#endif
#if TC_ENABLE_EAC_CVC && !TC_ENABLE_DER
#error "EAC CVC parsing requires TC_ENABLE_DER"
#endif
#ifndef TC_ENABLE_PIV_CVC
#define TC_ENABLE_PIV_CVC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_X509
#define TC_ENABLE_X509 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_X509 != 0 && TC_ENABLE_X509 != 1
#error "TC_ENABLE_X509 must be 0 or 1"
#endif
#if TC_ENABLE_X509 && !TC_ENABLE_DER
#error "X.509 parsing requires DER"
#endif
#ifndef TC_ENABLE_KEY_CHALLENGE
#define TC_ENABLE_KEY_CHALLENGE TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_KEY_CHALLENGE != 0 && TC_ENABLE_KEY_CHALLENGE != 1
#error "TC_ENABLE_KEY_CHALLENGE must be 0 or 1"
#endif
#if TC_ENABLE_KEY_CHALLENGE && !TC_ENABLE_X509
#error "Key challenges require X.509 public-key metadata"
#endif
#ifndef TC_ENABLE_GZIP
#define TC_ENABLE_GZIP TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_GZIP != 0 && TC_ENABLE_GZIP != 1
#error "TC_ENABLE_GZIP must be 0 or 1"
#endif
#ifndef TC_ENABLE_MD5
#define TC_ENABLE_MD5 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_MD5 != 0 && TC_ENABLE_MD5 != 1
#error "TC_ENABLE_MD5 must be 0 or 1"
#endif
#ifndef TC_ENABLE_TWIC_CCL
#define TC_ENABLE_TWIC_CCL TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_TWIC_CCL != 0 && TC_ENABLE_TWIC_CCL != 1
#error "TC_ENABLE_TWIC_CCL must be 0 or 1"
#endif

/* Standalone credential formats can be selected without certificate parsing. */
#ifndef TC_ENABLE_AAMVA
#define TC_ENABLE_AAMVA TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_FASCN
#define TC_ENABLE_FASCN TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_TWIC_UUID
#define TC_ENABLE_TWIC_UUID TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_TWIC_TPK
#define TC_ENABLE_TWIC_TPK TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if (TC_ENABLE_AAMVA != 0 && TC_ENABLE_AAMVA != 1) || \
    (TC_ENABLE_FASCN != 0 && TC_ENABLE_FASCN != 1) || \
    (TC_ENABLE_TWIC_UUID != 0 && TC_ENABLE_TWIC_UUID != 1) || \
    (TC_ENABLE_TWIC_TPK != 0 && TC_ENABLE_TWIC_TPK != 1)
#error "Standalone credential format switches must be 0 or 1"
#endif
#if TC_ENABLE_TWIC_UUID && !TC_ENABLE_FASCN
#error "TWIC UUID matching requires FASC-N support"
#endif
#if TC_ENABLE_TWIC_TPK && !TC_ENABLE_TLV
#error "TWIC TPK parsing requires TLV support"
#endif

#ifndef TC_ENABLE_PIV_CHUID
#define TC_ENABLE_PIV_CHUID TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_CHUID != 0 && TC_ENABLE_PIV_CHUID != 1
#error "TC_ENABLE_PIV_CHUID must be 0 or 1"
#endif
#if TC_ENABLE_PIV_CHUID && !TC_ENABLE_TLV
#error "CHUID parsing requires TLV"
#endif
#if TC_ENABLE_PIV_CVC != 0 && TC_ENABLE_PIV_CVC != 1
#error "TC_ENABLE_PIV_CVC must be 0 or 1"
#endif
#if TC_ENABLE_PIV_CVC && !TC_ENABLE_DER
#error "PIV CVC parsing requires DER"
#endif
#ifndef TC_TLV_ENABLE_BER
#define TC_TLV_ENABLE_BER TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_TLV_ENABLE_STREAM
#define TC_TLV_ENABLE_STREAM TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if (TC_ENABLE_TLV != 0 && TC_ENABLE_TLV != 1) || \
    (TC_ENABLE_DER != 0 && TC_ENABLE_DER != 1) || \
    (TC_TLV_ENABLE_BER != 0 && TC_TLV_ENABLE_BER != 1) || \
    (TC_TLV_ENABLE_STREAM != 0 && TC_TLV_ENABLE_STREAM != 1)
#error "TLV feature switches must be 0 or 1"
#endif
#if !TC_ENABLE_TLV && (TC_ENABLE_DER || TC_TLV_ENABLE_BER || TC_TLV_ENABLE_STREAM)
#error "DER, BER, and incremental parsing require TC_ENABLE_TLV"
#endif

/* Certificate processing layers. TC_ENABLE_X509 covers borrowed certificate
 * views; path, revocation, CMS, CMS validation, and credential composition are
 * opt-in. */
#ifndef TC_ENABLE_PIV_OIDS
#define TC_ENABLE_PIV_OIDS TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_X509_PATH
#define TC_ENABLE_X509_PATH TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_X509_REVOCATION
#define TC_ENABLE_X509_REVOCATION TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_CMS
#define TC_ENABLE_CMS TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_CMS_VALIDATION
#define TC_ENABLE_CMS_VALIDATION TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_PIV_OBJECTS
#define TC_ENABLE_PIV_OBJECTS TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_CREDENTIAL
#define TC_ENABLE_CREDENTIAL TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if (TC_ENABLE_PIV_OIDS != 0 && TC_ENABLE_PIV_OIDS != 1) || \
    (TC_ENABLE_X509_PATH != 0 && TC_ENABLE_X509_PATH != 1) || \
    (TC_ENABLE_X509_REVOCATION != 0 && TC_ENABLE_X509_REVOCATION != 1) || \
    (TC_ENABLE_CMS != 0 && TC_ENABLE_CMS != 1) || \
    (TC_ENABLE_CMS_VALIDATION != 0 && TC_ENABLE_CMS_VALIDATION != 1) || \
    (TC_ENABLE_PIV_OBJECTS != 0 && TC_ENABLE_PIV_OBJECTS != 1) || \
    (TC_ENABLE_CREDENTIAL != 0 && TC_ENABLE_CREDENTIAL != 1)
#error "Certificate layer switches must be 0 or 1"
#endif
#if TC_ENABLE_X509_PATH && !TC_ENABLE_X509
#error "X.509 path validation requires the X.509 reader"
#endif
#if TC_ENABLE_X509_REVOCATION && !TC_ENABLE_X509_PATH
#error "X.509 revocation requires path validation"
#endif
#if TC_ENABLE_CMS && (!TC_ENABLE_X509 || !TC_TLV_ENABLE_BER || !TC_ENABLE_PIV_OIDS)
#error "CMS requires X.509, BER parsing, and PIV/TWIC identifier classification"
#endif
#if TC_ENABLE_CMS_VALIDATION && (!TC_ENABLE_CMS || !TC_ENABLE_X509_REVOCATION)
#error "CMS validation requires CMS and X.509 revocation support"
#endif
#if TC_ENABLE_PIV_OBJECTS && \
    (!TC_ENABLE_CMS || !TC_ENABLE_TWIC_UUID || !TC_ENABLE_PIV_OIDS)
#error "PIV object readers require CMS, TWIC UUID, and PIV/TWIC identifiers"
#endif
#if TC_ENABLE_CREDENTIAL && \
    (!TC_ENABLE_PIV_OBJECTS || !TC_ENABLE_PIV_CHUID || !TC_ENABLE_CMS_VALIDATION)
#error "Credential composition requires PIV objects, CHUID, and CMS validation"
#endif
/* SP 800-108 KBKDF over the enabled HMAC / CMAC PRFs (kdf.c). */
#ifndef TC_ENABLE_KDF
#define TC_ENABLE_KDF TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_RSA
#define TC_ENABLE_RSA TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_RSA_SMALL
#define TC_RSA_SMALL TC_PROFILE_VALUE(0, 1, 0, 0)
#endif
#if (TC_ENABLE_RSA != 0 && TC_ENABLE_RSA != 1) || (TC_RSA_SMALL != 0 && TC_RSA_SMALL != 1)
#error "RSA options must be 0 or 1"
#endif

#ifndef TC_ENABLE_EC
#define TC_ENABLE_EC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_EC_ENABLE_P256
#define TC_EC_ENABLE_P256 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_EC_ENABLE_P192
#define TC_EC_ENABLE_P192 TC_PROFILE_VALUE(0, 0, 0, 0)
#endif
#ifndef TC_EC_ENABLE_P384
#define TC_EC_ENABLE_P384 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_EC_SMALL
#define TC_EC_SMALL TC_PROFILE_VALUE(0, 1, 0, 0)
#endif
#if (TC_ENABLE_EC != 0 && TC_ENABLE_EC != 1) || \
    (TC_EC_ENABLE_P192 != 0 && TC_EC_ENABLE_P192 != 1) || \
    (TC_EC_ENABLE_P256 != 0 && TC_EC_ENABLE_P256 != 1) || \
    (TC_EC_ENABLE_P384 != 0 && TC_EC_ENABLE_P384 != 1) || \
    (TC_EC_SMALL != 0 && TC_EC_SMALL != 1)
#error "EC switches must be 0 or 1"
#endif
#if TC_ENABLE_EC && !TC_EC_ENABLE_P192 && !TC_EC_ENABLE_P256 && !TC_EC_ENABLE_P384
#error "EC requires at least one curve"
#endif

#ifndef TC_ENABLE_SSKDF
#define TC_ENABLE_SSKDF TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_SSKDF != 0 && TC_ENABLE_SSKDF != 1
#error "TC_ENABLE_SSKDF must be 0 or 1"
#endif
#if TC_ENABLE_SSKDF && !TC_ENABLE_SHA256 && !TC_ENABLE_SHA384
#error "Single-step KDF requires SHA-256 or SHA-384"
#endif

/* Cross-algorithm security and embedded-storage policy.
 * TC_ZEROIZE: finalization wipes contexts and HMAC key-derived schedules,
 *   pads, and tags. Public-data hash schedules are not wiped on every block.
 *   CPU registers used as round working variables are not wiped.
 * TC_STRICT: streaming APIs reject NULL arguments (compiled out when 0).
 *   One-shot APIs always validate their arguments.
 * TC_AVR_PROGMEM: keep constant tables in AVR flash; 0 copies them into SRAM. */
#ifndef TC_ZEROIZE
#define TC_ZEROIZE TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_STRICT
#define TC_STRICT TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_AVR_PROGMEM
#define TC_AVR_PROGMEM TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#if (TC_AVR_PROGMEM != 0) && (TC_AVR_PROGMEM != 1)
#error "TC_AVR_PROGMEM must be 0 or 1"
#endif
#ifndef TC_HMAC_MIN_TAG_LEN
#define TC_HMAC_MIN_TAG_LEN 16
#endif

/* AES defaults favor small constant-time firmware: one key schedule size,
 * CTR only, no authentication-mode workspaces or lookup tables. */
#ifndef TC_AES_KEY_BITS
#define TC_AES_KEY_BITS 128
#endif
#ifndef TC_AES_ENABLE_DYNAMIC
#define TC_AES_ENABLE_DYNAMIC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_AES_ENABLE_DYNAMIC != 0 && TC_AES_ENABLE_DYNAMIC != 1
#error "TC_AES_ENABLE_DYNAMIC must be 0 or 1"
#endif
#if TC_AES_ENABLE_DYNAMIC && !TC_ENABLE_AES
#error "Dynamic AES requires TC_ENABLE_AES"
#endif
#ifndef TC_AES_ENABLE_CBC
#define TC_AES_ENABLE_CBC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_ECB
#define TC_AES_ENABLE_ECB TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_CTR
#define TC_AES_ENABLE_CTR TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_AES_ENABLE_OFB
#define TC_AES_ENABLE_OFB TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_GCM
#define TC_AES_ENABLE_GCM TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_CCM
#define TC_AES_ENABLE_CCM TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_EAX
#define TC_AES_ENABLE_EAX TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_EAX_PRIME
#define TC_AES_ENABLE_EAX_PRIME TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_SIV
#define TC_AES_ENABLE_SIV TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_ENABLE_CMAC
#define TC_AES_ENABLE_CMAC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_EAX_MIN_TAG_LEN
#define TC_AES_EAX_MIN_TAG_LEN 8
#endif
#ifndef TC_AES_CMAC_MIN_TAG_LEN
#define TC_AES_CMAC_MIN_TAG_LEN 8
#endif
#ifndef TC_AES_TINY
#define TC_AES_TINY TC_PROFILE_VALUE(0, 1, 0, 0)
#endif
#ifndef TC_AES_GCM_GHASH_MODE
#define TC_AES_GCM_GHASH_MODE TC_PROFILE_VALUE(0, 1, 0, 2)
#endif
#ifndef TC_AES_SBOX_MODE
#define TC_AES_SBOX_MODE 1
#endif
#ifndef TC_AES_WIDE_OPS
#define TC_AES_WIDE_OPS TC_PROFILE_VALUE(0, 0, 1, 1)
#endif

#ifndef TC_ENABLE_TWIC_OBJECT_CRYPTO
#define TC_ENABLE_TWIC_OBJECT_CRYPTO TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_TWIC_OBJECT_CRYPTO != 0 && TC_ENABLE_TWIC_OBJECT_CRYPTO != 1
#error "TC_ENABLE_TWIC_OBJECT_CRYPTO must be 0 or 1"
#endif
#if TC_ENABLE_TWIC_OBJECT_CRYPTO && \
    (!TC_ENABLE_AES || !TC_AES_ENABLE_ECB || TC_AES_KEY_BITS != 128)
#error "TWIC object encryption requires AES-128 ECB"
#endif

/* DES defaults preserve the imported project's CTR and 3DES behavior. DES has
 * a 56-bit effective key and should only be used for legacy interoperability. */
#ifndef TC_DES_ENABLE_ECB
#define TC_DES_ENABLE_ECB TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_ENABLE_CBC
#define TC_DES_ENABLE_CBC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_ENABLE_CTR
#define TC_DES_ENABLE_CTR TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_DES_ENABLE_OFB
#define TC_DES_ENABLE_OFB TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_ENABLE_CFB1
#define TC_DES_ENABLE_CFB1 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_ENABLE_CFB8
#define TC_DES_ENABLE_CFB8 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_ENABLE_CFB64
#define TC_DES_ENABLE_CFB64 TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_ENABLE_TDES
#define TC_DES_ENABLE_TDES TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_DES_ENABLE_CMAC
#define TC_DES_ENABLE_CMAC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DES_REJECT_WEAK_KEYS
#define TC_DES_REJECT_WEAK_KEYS TC_PROFILE_VALUE(0, 0, 0, 0)
#endif

#ifndef TC_ENABLE_PIV_SM
#define TC_ENABLE_PIV_SM TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_PIV_SM_ENABLE_CS2
#define TC_PIV_SM_ENABLE_CS2 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_PIV_SM_ENABLE_CS7
#define TC_PIV_SM_ENABLE_CS7 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#if (TC_ENABLE_PIV_SM != 0 && TC_ENABLE_PIV_SM != 1) || \
    (TC_PIV_SM_ENABLE_CS2 != 0 && TC_PIV_SM_ENABLE_CS2 != 1) || \
    (TC_PIV_SM_ENABLE_CS7 != 0 && TC_PIV_SM_ENABLE_CS7 != 1)
#error "PIV SM switches must be 0 or 1"
#endif
#if TC_ENABLE_PIV_SM
#if !TC_ENABLE_AES || !TC_AES_ENABLE_DYNAMIC || !TC_ENABLE_SHA256 || \
    !TC_ENABLE_SSKDF || !TC_ENABLE_EC
#error "PIV SM requires dynamic AES, SHA-256, single-step KDF, and EC"
#endif
#if !TC_PIV_SM_ENABLE_CS2 && !TC_PIV_SM_ENABLE_CS7
#error "PIV SM requires at least one cipher suite"
#endif
#if TC_PIV_SM_ENABLE_CS2 && !TC_EC_ENABLE_P256
#error "CS2 requires P-256"
#endif
#if TC_PIV_SM_ENABLE_CS7 && (!TC_EC_ENABLE_P384 || !TC_ENABLE_SHA384)
#error "CS7 requires P-384 and SHA-384"
#endif
#endif

#endif
