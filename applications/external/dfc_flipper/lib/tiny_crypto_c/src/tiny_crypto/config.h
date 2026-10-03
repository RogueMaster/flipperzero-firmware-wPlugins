/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* Build configuration. Each TC_* macro selects an algorithm, mode or
 * implementation, with defaults from the resource profile in
 * resource_profile.h. The checks at the end stop the build with an #error
 * for a value outside its range or a missing dependency.
 * Configuration: cmake/features.json registers each feature macro here. Its
 * CMake option is TINY_CRYPTO_ followed by the macro name without TC_.
 * Contracts: docs/api.md. Options: README.md. */
#ifndef TINY_CRYPTO_CONFIG_H_
#define TINY_CRYPTO_CONFIG_H_
/* An installed library records its configuration here, so installed headers
 * match the archive without -D definitions. The quoted form selects the file
 * beside this header. A second include at the end of this header checks
 * consumer definitions against the recorded values. */
#include "build_config.h"
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
#if TC_ENABLE_AES != 0 && TC_ENABLE_AES != 1
#error "TC_ENABLE_AES must be 0 or 1"
#endif
#if TC_ENABLE_DES != 0 && TC_ENABLE_DES != 1
#error "TC_ENABLE_DES must be 0 or 1"
#endif
#if TC_ENABLE_SHA1 != 0 && TC_ENABLE_SHA1 != 1
#error "TC_ENABLE_SHA1 must be 0 or 1"
#endif
#if TC_ENABLE_SHA224 != 0 && TC_ENABLE_SHA224 != 1
#error "TC_ENABLE_SHA224 must be 0 or 1"
#endif
#if TC_ENABLE_SHA256 != 0 && TC_ENABLE_SHA256 != 1
#error "TC_ENABLE_SHA256 must be 0 or 1"
#endif
#if TC_ENABLE_SHA384 != 0 && TC_ENABLE_SHA384 != 1
#error "TC_ENABLE_SHA384 must be 0 or 1"
#endif
#if TC_ENABLE_SHA512 != 0 && TC_ENABLE_SHA512 != 1
#error "TC_ENABLE_SHA512 must be 0 or 1"
#endif
#if TC_ENABLE_HMAC != 0 && TC_ENABLE_HMAC != 1
#error "TC_ENABLE_HMAC must be 0 or 1"
#endif
#if TC_ENABLE_KMAC256 != 0 && TC_ENABLE_KMAC256 != 1
#error "TC_ENABLE_KMAC256 must be 0 or 1"
#endif
#if TC_ENABLE_HMAC && !(TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 ||                  \
                        TC_ENABLE_SHA384 || TC_ENABLE_SHA512)
#error "HMAC requires an enabled SHA algorithm"
#endif
/* TLV framing is independent of the cryptographic algorithms. DER adds typed
 * value checks; BER and incremental entry points are optional. Parser bounds
 * checks are always on. */
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
#if TC_ENABLE_PIV_CVC != 0 && TC_ENABLE_PIV_CVC != 1
#error "TC_ENABLE_PIV_CVC must be 0 or 1"
#endif
#if TC_ENABLE_PIV_CVC && !TC_ENABLE_DER
#error "PIV CVC parsing requires DER"
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
#if (TC_ENABLE_AAMVA != 0 && TC_ENABLE_AAMVA != 1) ||                                              \
    (TC_ENABLE_FASCN != 0 && TC_ENABLE_FASCN != 1) ||                                              \
    (TC_ENABLE_TWIC_UUID != 0 && TC_ENABLE_TWIC_UUID != 1) ||                                      \
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
#ifndef TC_TLV_ENABLE_BER
#define TC_TLV_ENABLE_BER TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_TLV_ENABLE_STREAM
#define TC_TLV_ENABLE_STREAM TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if (TC_ENABLE_TLV != 0 && TC_ENABLE_TLV != 1) || (TC_ENABLE_DER != 0 && TC_ENABLE_DER != 1) ||    \
    (TC_TLV_ENABLE_BER != 0 && TC_TLV_ENABLE_BER != 1) ||                                          \
    (TC_TLV_ENABLE_STREAM != 0 && TC_TLV_ENABLE_STREAM != 1)
#error "TLV feature switches must be 0 or 1"
#endif
#if !TC_ENABLE_TLV && (TC_ENABLE_DER || TC_TLV_ENABLE_BER || TC_TLV_ENABLE_STREAM)
#error "DER, BER, and incremental parsing require TC_ENABLE_TLV"
#endif
/* ISO/IEC 7816-4 command and response APDUs with a bounded exchange channel.
 * The codec depends on no other module. */
#ifndef TC_ENABLE_APDU
#define TC_ENABLE_APDU TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_APDU != 0 && TC_ENABLE_APDU != 1
#error "TC_ENABLE_APDU must be 0 or 1"
#endif
/* PIV and TWIC card commands (SELECT, GET DATA, VERIFY) over the APDU
 * channel. */
#ifndef TC_ENABLE_PIV_COMMAND
#define TC_ENABLE_PIV_COMMAND TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_COMMAND != 0 && TC_ENABLE_PIV_COMMAND != 1
#error "TC_ENABLE_PIV_COMMAND must be 0 or 1"
#endif
#if TC_ENABLE_PIV_COMMAND && (!TC_ENABLE_APDU || !TC_ENABLE_TLV)
#error "PIV card commands require the APDU codec and TLV readers"
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
#ifndef TC_ENABLE_TRUST_ANCHOR_FORMAT
#define TC_ENABLE_TRUST_ANCHOR_FORMAT TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_TAF_ENABLE_CERTIFICATE
#define TC_TAF_ENABLE_CERTIFICATE TC_ENABLE_TRUST_ANCHOR_FORMAT
#endif
#ifndef TC_TAF_ENABLE_TBS_CERTIFICATE
#define TC_TAF_ENABLE_TBS_CERTIFICATE TC_ENABLE_TRUST_ANCHOR_FORMAT
#endif
#ifndef TC_TAF_ENABLE_TRUST_ANCHOR_INFO
#define TC_TAF_ENABLE_TRUST_ANCHOR_INFO TC_ENABLE_TRUST_ANCHOR_FORMAT
#endif
#if (TC_ENABLE_TRUST_ANCHOR_FORMAT != 0 && TC_ENABLE_TRUST_ANCHOR_FORMAT != 1) ||                  \
    (TC_TAF_ENABLE_CERTIFICATE != 0 && TC_TAF_ENABLE_CERTIFICATE != 1) ||                          \
    (TC_TAF_ENABLE_TBS_CERTIFICATE != 0 && TC_TAF_ENABLE_TBS_CERTIFICATE != 1) ||                  \
    (TC_TAF_ENABLE_TRUST_ANCHOR_INFO != 0 && TC_TAF_ENABLE_TRUST_ANCHOR_INFO != 1)
#error "Trust-anchor format switches must be 0 or 1"
#endif
#if TC_ENABLE_TRUST_ANCHOR_FORMAT && !TC_ENABLE_X509_PATH
#error "Trust-anchor format requires X.509 path support"
#endif
#if TC_ENABLE_TRUST_ANCHOR_FORMAT && !TC_TAF_ENABLE_CERTIFICATE &&                                 \
    !TC_TAF_ENABLE_TBS_CERTIFICATE && !TC_TAF_ENABLE_TRUST_ANCHOR_INFO
#error "Trust-anchor format requires at least one choice"
#endif
#if !TC_ENABLE_TRUST_ANCHOR_FORMAT &&                                                              \
    (TC_TAF_ENABLE_CERTIFICATE || TC_TAF_ENABLE_TBS_CERTIFICATE ||                                 \
     TC_TAF_ENABLE_TRUST_ANCHOR_INFO)
#error "Trust-anchor choices require trust-anchor format"
#endif
#ifndef TC_ENABLE_X509_REVOCATION
#define TC_ENABLE_X509_REVOCATION TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_ENABLE_X509_OCSP
#define TC_ENABLE_X509_OCSP TC_PROFILE_VALUE(0, 0, 0, 1)
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
#if (TC_ENABLE_PIV_OIDS != 0 && TC_ENABLE_PIV_OIDS != 1) ||                                        \
    (TC_ENABLE_X509_PATH != 0 && TC_ENABLE_X509_PATH != 1) ||                                      \
    (TC_ENABLE_X509_REVOCATION != 0 && TC_ENABLE_X509_REVOCATION != 1) ||                          \
    (TC_ENABLE_X509_OCSP != 0 && TC_ENABLE_X509_OCSP != 1) ||                                      \
    (TC_ENABLE_CMS != 0 && TC_ENABLE_CMS != 1) ||                                                  \
    (TC_ENABLE_CMS_VALIDATION != 0 && TC_ENABLE_CMS_VALIDATION != 1) ||                            \
    (TC_ENABLE_PIV_OBJECTS != 0 && TC_ENABLE_PIV_OBJECTS != 1) ||                                  \
    (TC_ENABLE_CREDENTIAL != 0 && TC_ENABLE_CREDENTIAL != 1)
#error "Certificate layer switches must be 0 or 1"
#endif
#if TC_ENABLE_X509_PATH && !TC_ENABLE_X509
#error "X.509 path validation requires the X.509 reader"
#endif
#if TC_ENABLE_X509_REVOCATION && !TC_ENABLE_X509_PATH
#error "X.509 revocation requires path validation"
#endif
#if TC_ENABLE_X509_OCSP && !TC_ENABLE_X509_PATH
#error "X.509 OCSP requires path validation"
#endif
/* A byKey ResponderID is a SHA-1 key hash (RFC 6960 4.2.1). */
#if TC_ENABLE_X509_OCSP && !TC_ENABLE_SHA1
#error "X.509 OCSP requires SHA-1"
#endif
#if TC_ENABLE_CMS && (!TC_ENABLE_X509 || !TC_TLV_ENABLE_BER || !TC_ENABLE_PIV_OIDS)
#error "CMS requires X.509, BER parsing, and PIV/TWIC identifier classification"
#endif
#if TC_ENABLE_CMS_VALIDATION && (!TC_ENABLE_CMS || !TC_ENABLE_X509_REVOCATION)
#error "CMS validation requires CMS and X.509 revocation support"
#endif
#if TC_ENABLE_PIV_OBJECTS && (!TC_ENABLE_CMS || !TC_ENABLE_TWIC_UUID || !TC_ENABLE_PIV_OIDS)
#error "PIV object readers require CMS, TWIC UUID, and PIV/TWIC identifiers"
#endif
#if TC_ENABLE_CREDENTIAL &&                                                                        \
    (!TC_ENABLE_PIV_OBJECTS || !TC_ENABLE_PIV_CHUID || !TC_ENABLE_CMS_VALIDATION)
#error "Credential composition requires PIV objects, CHUID, and CMS validation"
#endif
/* SP 800-108r1 KBKDF over the enabled HMAC / CMAC PRFs (kdf.c). */
#ifndef TC_ENABLE_KDF
#define TC_ENABLE_KDF TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_KDF != 0 && TC_ENABLE_KDF != 1
#error "TC_ENABLE_KDF must be 0 or 1"
#endif
/* RFC 5869 HKDF over the enabled HMAC-SHA algorithms. */
#ifndef TC_ENABLE_HKDF
#define TC_ENABLE_HKDF TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_HKDF != 0 && TC_ENABLE_HKDF != 1
#error "TC_ENABLE_HKDF must be 0 or 1"
#endif
#if TC_ENABLE_HKDF &&                                                                              \
    (!TC_ENABLE_HMAC || !(TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 ||                \
                          TC_ENABLE_SHA384 || TC_ENABLE_SHA512))
#error "HKDF requires HMAC and an enabled SHA algorithm"
#endif
#ifndef TC_ENABLE_RSA
#define TC_ENABLE_RSA TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_RSA_SMALL
#define TC_RSA_SMALL TC_PROFILE_VALUE(0, 1, 0, 0)
#endif
#ifndef TC_RSA_ENABLE_1024
#define TC_RSA_ENABLE_1024 TC_PROFILE_VALUE(0, 0, 0, 0)
#endif
#ifndef TC_RSA_ENABLE_2048
#define TC_RSA_ENABLE_2048 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_RSA_ENABLE_3072
#define TC_RSA_ENABLE_3072 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#ifndef TC_RSA_ENABLE_4096
#define TC_RSA_ENABLE_4096 TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#if (TC_ENABLE_RSA != 0 && TC_ENABLE_RSA != 1) || (TC_RSA_SMALL != 0 && TC_RSA_SMALL != 1) ||      \
    (TC_RSA_ENABLE_1024 != 0 && TC_RSA_ENABLE_1024 != 1) ||                                        \
    (TC_RSA_ENABLE_2048 != 0 && TC_RSA_ENABLE_2048 != 1) ||                                        \
    (TC_RSA_ENABLE_3072 != 0 && TC_RSA_ENABLE_3072 != 1) ||                                        \
    (TC_RSA_ENABLE_4096 != 0 && TC_RSA_ENABLE_4096 != 1)
#error "RSA options must be 0 or 1"
#endif
#if TC_ENABLE_RSA &&                                                                               \
    !(TC_RSA_ENABLE_1024 || TC_RSA_ENABLE_2048 || TC_RSA_ENABLE_3072 || TC_RSA_ENABLE_4096)
#error "RSA requires at least one enabled modulus size"
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
#if (TC_ENABLE_EC != 0 && TC_ENABLE_EC != 1) ||                                                    \
    (TC_EC_ENABLE_P192 != 0 && TC_EC_ENABLE_P192 != 1) ||                                          \
    (TC_EC_ENABLE_P256 != 0 && TC_EC_ENABLE_P256 != 1) ||                                          \
    (TC_EC_ENABLE_P384 != 0 && TC_EC_ENABLE_P384 != 1) || (TC_EC_SMALL != 0 && TC_EC_SMALL != 1)
#error "EC switches must be 0 or 1"
#endif
#if TC_ENABLE_EC && !TC_EC_ENABLE_P192 && !TC_EC_ENABLE_P256 && !TC_EC_ENABLE_P384
#error "EC requires at least one curve"
#endif
/* Verify each ECDSA signature before returning it, to catch faults during
 * signing. */
#ifndef TC_ECDSA_SIGN_VERIFY
#define TC_ECDSA_SIGN_VERIFY 1
#endif
#if TC_ECDSA_SIGN_VERIFY != 0 && TC_ECDSA_SIGN_VERIFY != 1
#error "TC_ECDSA_SIGN_VERIFY must be 0 or 1"
#endif

#ifndef TC_ENABLE_SSKDF
#define TC_ENABLE_SSKDF TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_SSKDF != 0 && TC_ENABLE_SSKDF != 1
#error "TC_ENABLE_SSKDF must be 0 or 1"
#endif
#if TC_ENABLE_SSKDF && !(TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 ||                 \
                         TC_ENABLE_SHA384 || TC_ENABLE_SHA512)
#error "Single-step KDF requires an enabled SHA algorithm"
#endif

/* NIST SP 800-90A deterministic random bit generators. Each mechanism is
 * compiled only when TC_ENABLE_DRBG is set. */
#ifndef TC_ENABLE_DRBG
#define TC_ENABLE_DRBG TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DRBG_ENABLE_HASH
#define TC_DRBG_ENABLE_HASH TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DRBG_ENABLE_HMAC
#define TC_DRBG_ENABLE_HMAC TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_DRBG_ENABLE_CTR
#define TC_DRBG_ENABLE_CTR TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if (TC_ENABLE_DRBG != 0 && TC_ENABLE_DRBG != 1) ||                                                \
    (TC_DRBG_ENABLE_HASH != 0 && TC_DRBG_ENABLE_HASH != 1) ||                                      \
    (TC_DRBG_ENABLE_HMAC != 0 && TC_DRBG_ENABLE_HMAC != 1) ||                                      \
    (TC_DRBG_ENABLE_CTR != 0 && TC_DRBG_ENABLE_CTR != 1)
#error "DRBG switches must be 0 or 1"
#endif
/* Largest DRBG entropy input per instantiate or reseed, in bytes, including a
 * nonce drawn from the entropy source. The default covers every mechanism at
 * its full strength. CTR_DRBG with AES-256 needs a 48-byte seed without a
 * derivation function (SP 800-90A Table 3). */
#ifndef TC_DRBG_MAX_ENTROPY_BYTES
#define TC_DRBG_MAX_ENTROPY_BYTES 64u
#endif
#if TC_ENABLE_DRBG && TC_DRBG_MAX_ENTROPY_BYTES < 48u
#error "TC_DRBG_MAX_ENTROPY_BYTES must hold a CTR_DRBG seed (48 bytes)"
#endif

/* Secret wiping and public argument checks are always enabled. Every final,
 * one-shot and failure path wipes key-dependent state, and every public entry
 * validates its pointers. CPU registers used as round working variables are
 * not wiped. Defining TC_ZEROIZE or TC_STRICT is an error because neither
 * behavior can be disabled.
 * TC_AVR_PROGMEM: keep constant tables in AVR flash. 0 copies them into SRAM. */
#if defined(TC_ZEROIZE) || defined(TC_STRICT)
#error "TC_ZEROIZE and TC_STRICT are removed: wiping and argument checks are always on"
#endif
#ifndef TC_AVR_PROGMEM
#define TC_AVR_PROGMEM TC_PROFILE_VALUE(1, 1, 1, 1)
#endif
#if TC_AVR_PROGMEM != 0 && TC_AVR_PROGMEM != 1
#error "TC_AVR_PROGMEM must be 0 or 1"
#endif
/* Minimum tag length in bytes for the default CCM, EAX, AES-CMAC and
 * DES-CMAC entry points. SP 800-38B Appendix A.2 asks for at least 64 bits
 * unless a protocol limits failed verifications. The value may be raised to
 * 16. Protocols with shorter tags call the explicit _short_tag entry points,
 * which take the lengths below this minimum. GCM keeps its SP 800-38D
 * lengths, and the EAX' tag is fixed at 4 bytes by ANSI C12.22. */
#if defined(TC_AES_EAX_MIN_TAG_LEN) || defined(TC_AES_CMAC_MIN_TAG_LEN) ||                         \
    defined(TC_DES_CMAC_MIN_TAG_LEN)
#error "Per-mode minimum tag lengths are removed: set TC_MIN_TAG_LEN or call the _short_tag APIs"
#endif
#ifndef TC_MIN_TAG_LEN
#define TC_MIN_TAG_LEN 8
#endif
#if TC_MIN_TAG_LEN < 8 || TC_MIN_TAG_LEN > 16
#error "TC_MIN_TAG_LEN must be in 8..16"
#endif
/* Minimum accepted HMAC tag length for the one-shot HMAC and verify APIs.
 * RFC 2104 section 5 asks for at least half the digest and at least 80 bits.
 * It may not exceed the digest length of any enabled SHA. */
#ifndef TC_HMAC_MIN_TAG_LEN
#define TC_HMAC_MIN_TAG_LEN 16
#endif
#if TC_ENABLE_HMAC && (TC_HMAC_MIN_TAG_LEN < 1 || (TC_ENABLE_SHA1 && TC_HMAC_MIN_TAG_LEN > 20) ||  \
                       (TC_ENABLE_SHA224 && TC_HMAC_MIN_TAG_LEN > 28) ||                           \
                       (TC_ENABLE_SHA256 && TC_HMAC_MIN_TAG_LEN > 32) ||                           \
                       (TC_ENABLE_SHA384 && TC_HMAC_MIN_TAG_LEN > 48) ||                           \
                       (TC_ENABLE_SHA512 && TC_HMAC_MIN_TAG_LEN > 64))
#error "TC_HMAC_MIN_TAG_LEN must be at least 1 and at most every enabled SHA digest length"
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
/* SP 800-38F KW and KWP key wrap (aes_kw.c). */
#ifndef TC_AES_ENABLE_KW
#define TC_AES_ENABLE_KW TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#ifndef TC_AES_TINY
#define TC_AES_TINY TC_PROFILE_VALUE(0, 1, 0, 0)
#endif
/* Single-block AES known-answer entry points used by the CAVP harness.
 * Production builds leave them off. */
#ifndef TC_AES_CAVP
#define TC_AES_CAVP 0
#endif
#if TC_AES_CAVP != 0 && TC_AES_CAVP != 1
#error "TC_AES_CAVP must be 0 or 1"
#endif
/* GCM GHASH implementation profiles. */
#define TC_AES_GCM_GHASH_MODE_AUTO 0
#define TC_AES_GCM_GHASH_MODE_BITWISE 1
#define TC_AES_GCM_GHASH_MODE_WIDE 2
#define TC_AES_GCM_GHASH_MODE_FAST_TABLE 3
#define TC_AES_GCM_GHASH_MODE_HARDWARE 4
#ifndef TC_AES_GCM_GHASH_MODE
#define TC_AES_GCM_GHASH_MODE TC_PROFILE_VALUE(0, 1, 0, 2)
#endif
/* S-box implementations:
 *   TC_AES_SBOX_MODE_CONSTANT_TIME - algebraic inversion (default)
 *   TC_AES_SBOX_MODE_RUNTIME       - generated in RAM, then masked scan
 *   TC_AES_SBOX_MODE_FAST          - direct lookup, not constant-time */
#define TC_AES_SBOX_MODE_CONSTANT_TIME 1
#define TC_AES_SBOX_MODE_RUNTIME 2
#define TC_AES_SBOX_MODE_FAST 3
#ifndef TC_AES_SBOX_MODE
#define TC_AES_SBOX_MODE TC_AES_SBOX_MODE_CONSTANT_TIME
#endif
/* 0 keeps byte-safe operations. 1 enables portable native-width helpers. */
#ifndef TC_AES_WIDE_OPS
#define TC_AES_WIDE_OPS TC_PROFILE_VALUE(0, 0, 1, 1)
#endif
#if TC_AES_ENABLE_CBC != 0 && TC_AES_ENABLE_CBC != 1
#error "TC_AES_ENABLE_CBC must be 0 or 1"
#endif
#if TC_AES_ENABLE_ECB != 0 && TC_AES_ENABLE_ECB != 1
#error "TC_AES_ENABLE_ECB must be 0 or 1"
#endif
#if TC_AES_ENABLE_CTR != 0 && TC_AES_ENABLE_CTR != 1
#error "TC_AES_ENABLE_CTR must be 0 or 1"
#endif
#if TC_AES_ENABLE_OFB != 0 && TC_AES_ENABLE_OFB != 1
#error "TC_AES_ENABLE_OFB must be 0 or 1"
#endif
#if TC_AES_ENABLE_GCM != 0 && TC_AES_ENABLE_GCM != 1
#error "TC_AES_ENABLE_GCM must be 0 or 1"
#endif
#if TC_AES_ENABLE_CCM != 0 && TC_AES_ENABLE_CCM != 1
#error "TC_AES_ENABLE_CCM must be 0 or 1"
#endif
#if TC_AES_ENABLE_EAX != 0 && TC_AES_ENABLE_EAX != 1
#error "TC_AES_ENABLE_EAX must be 0 or 1"
#endif
#if TC_AES_ENABLE_EAX_PRIME != 0 && TC_AES_ENABLE_EAX_PRIME != 1
#error "TC_AES_ENABLE_EAX_PRIME must be 0 or 1"
#endif
#if TC_AES_ENABLE_SIV != 0 && TC_AES_ENABLE_SIV != 1
#error "TC_AES_ENABLE_SIV must be 0 or 1"
#endif
#if TC_AES_ENABLE_CMAC != 0 && TC_AES_ENABLE_CMAC != 1
#error "TC_AES_ENABLE_CMAC must be 0 or 1"
#endif
#if TC_AES_ENABLE_KW != 0 && TC_AES_ENABLE_KW != 1
#error "TC_AES_ENABLE_KW must be 0 or 1"
#endif
#if TC_AES_TINY != 0 && TC_AES_TINY != 1
#error "TC_AES_TINY must be 0 or 1"
#endif
#if TC_AES_WIDE_OPS != 0 && TC_AES_WIDE_OPS != 1
#error "TC_AES_WIDE_OPS must be 0 or 1"
#endif
/* Exactly one AES key schedule size per library profile. */
#if TC_AES_KEY_BITS != 128 && TC_AES_KEY_BITS != 192 && TC_AES_KEY_BITS != 256
#error "TC_AES_KEY_BITS must be 128, 192, or 256"
#endif
#if TC_AES_GCM_GHASH_MODE < TC_AES_GCM_GHASH_MODE_AUTO ||                                          \
    TC_AES_GCM_GHASH_MODE > TC_AES_GCM_GHASH_MODE_HARDWARE
#error "TC_AES_GCM_GHASH_MODE is invalid"
#endif
/* TC_AES_TINY rejects the per-key 256-byte fast GHASH table. */
#if TC_AES_TINY && TC_AES_GCM_GHASH_MODE == TC_AES_GCM_GHASH_MODE_FAST_TABLE
#error "TC_AES_TINY forbids the 256-byte fast GHASH table"
#endif
#if TC_AES_SBOX_MODE < TC_AES_SBOX_MODE_CONSTANT_TIME || TC_AES_SBOX_MODE > TC_AES_SBOX_MODE_FAST
#error "TC_AES_SBOX_MODE must be TC_AES_SBOX_MODE_CONSTANT_TIME, _RUNTIME, or _FAST"
#endif

#ifndef TC_ENABLE_TWIC_OBJECT_CRYPTO
#define TC_ENABLE_TWIC_OBJECT_CRYPTO TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_TWIC_OBJECT_CRYPTO != 0 && TC_ENABLE_TWIC_OBJECT_CRYPTO != 1
#error "TC_ENABLE_TWIC_OBJECT_CRYPTO must be 0 or 1"
#endif
#if TC_ENABLE_TWIC_OBJECT_CRYPTO && (!TC_ENABLE_AES || !TC_AES_ENABLE_ECB || TC_AES_KEY_BITS != 128)
#error "TWIC object encryption requires AES-128 ECB"
#endif

/* DES defaults enable CTR and Triple DES only. DES has a 56-bit effective
 * key; use it only for legacy interoperability. */
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
#ifndef TC_DES_ENABLE_ISO9797
#define TC_DES_ENABLE_ISO9797 TC_PROFILE_VALUE(0, 0, 0, 0)
#endif
#ifndef TC_DES_REJECT_WEAK_KEYS
#define TC_DES_REJECT_WEAK_KEYS TC_PROFILE_VALUE(0, 0, 0, 0)
#endif
#if TC_DES_ENABLE_ECB != 0 && TC_DES_ENABLE_ECB != 1
#error "TC_DES_ENABLE_ECB must be 0 or 1"
#endif
#if TC_DES_ENABLE_CBC != 0 && TC_DES_ENABLE_CBC != 1
#error "TC_DES_ENABLE_CBC must be 0 or 1"
#endif
#if TC_DES_ENABLE_CTR != 0 && TC_DES_ENABLE_CTR != 1
#error "TC_DES_ENABLE_CTR must be 0 or 1"
#endif
#if TC_DES_ENABLE_OFB != 0 && TC_DES_ENABLE_OFB != 1
#error "TC_DES_ENABLE_OFB must be 0 or 1"
#endif
#if TC_DES_ENABLE_CFB1 != 0 && TC_DES_ENABLE_CFB1 != 1
#error "TC_DES_ENABLE_CFB1 must be 0 or 1"
#endif
#if TC_DES_ENABLE_CFB8 != 0 && TC_DES_ENABLE_CFB8 != 1
#error "TC_DES_ENABLE_CFB8 must be 0 or 1"
#endif
#if TC_DES_ENABLE_CFB64 != 0 && TC_DES_ENABLE_CFB64 != 1
#error "TC_DES_ENABLE_CFB64 must be 0 or 1"
#endif
#if TC_DES_ENABLE_TDES != 0 && TC_DES_ENABLE_TDES != 1
#error "TC_DES_ENABLE_TDES must be 0 or 1"
#endif
#if TC_DES_ENABLE_CMAC != 0 && TC_DES_ENABLE_CMAC != 1
#error "TC_DES_ENABLE_CMAC must be 0 or 1"
#endif
#if TC_DES_ENABLE_ISO9797 != 0 && TC_DES_ENABLE_ISO9797 != 1
#error "TC_DES_ENABLE_ISO9797 must be 0 or 1"
#endif
#if TC_DES_REJECT_WEAK_KEYS != 0 && TC_DES_REJECT_WEAK_KEYS != 1
#error "TC_DES_REJECT_WEAK_KEYS must be 0 or 1"
#endif
#if TC_ENABLE_DES && !TC_DES_ENABLE_ECB && !TC_DES_ENABLE_CBC && !TC_DES_ENABLE_CTR &&             \
    !TC_DES_ENABLE_OFB && !TC_DES_ENABLE_CFB1 && !TC_DES_ENABLE_CFB8 && !TC_DES_ENABLE_CFB64 &&    \
    !TC_DES_ENABLE_CMAC && !TC_DES_ENABLE_ISO9797
#error "DES requires at least one enabled mode or MAC"
#endif
/* A full DES-CMAC tag is one 8-byte block. A higher minimum would leave the
 * default DES-CMAC entry points with no accepted length. */
#if TC_ENABLE_DES && TC_DES_ENABLE_CMAC && TC_MIN_TAG_LEN > 8
#error "TC_MIN_TAG_LEN above 8 requires TC_DES_ENABLE_CMAC=0"
#endif
/* KBKDF needs a PRF: HMAC with an enabled SHA, AES-CMAC or DES-CMAC. */
#if TC_ENABLE_KDF && !TC_ENABLE_HMAC && !(TC_ENABLE_AES && TC_AES_ENABLE_CMAC) &&                  \
    !(TC_ENABLE_DES && TC_DES_ENABLE_CMAC)
#error                                                                                             \
    "TC_ENABLE_KDF needs a PRF: HMAC with an enabled SHA, TC_AES_ENABLE_CMAC or TC_DES_ENABLE_CMAC"
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
#if (TC_ENABLE_PIV_SM != 0 && TC_ENABLE_PIV_SM != 1) ||                                            \
    (TC_PIV_SM_ENABLE_CS2 != 0 && TC_PIV_SM_ENABLE_CS2 != 1) ||                                    \
    (TC_PIV_SM_ENABLE_CS7 != 0 && TC_PIV_SM_ENABLE_CS7 != 1)
#error "PIV SM switches must be 0 or 1"
#endif
#if TC_ENABLE_PIV_SM
#if !TC_ENABLE_AES || !TC_AES_ENABLE_DYNAMIC || !TC_ENABLE_SHA256 || !TC_ENABLE_SSKDF ||           \
    !TC_ENABLE_EC
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

/* PIV secure messaging framing on a PIV card link: key establishment,
 * protected commands and responses. */
#ifndef TC_ENABLE_PIV_SM_APDU
#define TC_ENABLE_PIV_SM_APDU TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_SM_APDU != 0 && TC_ENABLE_PIV_SM_APDU != 1
#error "TC_ENABLE_PIV_SM_APDU must be 0 or 1"
#endif
#if TC_ENABLE_PIV_SM_APDU && (!TC_ENABLE_PIV_COMMAND || !TC_ENABLE_PIV_SM || !TC_ENABLE_PIV_CVC)
#error "PIV secure messaging framing requires PIV card commands, PIV SM and PIV CVC"
#endif

/* PIV virtual contact interface: the Discovery Object read over the link and
 * the pairing-code VERIFY under secure messaging. */
#ifndef TC_ENABLE_PIV_VCI
#define TC_ENABLE_PIV_VCI TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_VCI != 0 && TC_ENABLE_PIV_VCI != 1
#error "TC_ENABLE_PIV_VCI must be 0 or 1"
#endif
#if TC_ENABLE_PIV_VCI && (!TC_ENABLE_PIV_SM_APDU || !TC_ENABLE_PIV_OBJECTS)
#error "The PIV virtual contact interface requires PIV secure messaging framing and PIV objects"
#endif

/* PIV and TWIC data object catalogs and the access-rule-aware inventory. */
#ifndef TC_ENABLE_PIV_CATALOG
#define TC_ENABLE_PIV_CATALOG TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_CATALOG != 0 && TC_ENABLE_PIV_CATALOG != 1
#error "TC_ENABLE_PIV_CATALOG must be 0 or 1"
#endif
#if TC_ENABLE_PIV_CATALOG && !TC_ENABLE_PIV_COMMAND
#error "The PIV catalog requires PIV card commands"
#endif

/* Key proofs: a card key signs a fresh challenge over GENERAL AUTHENTICATE
 * under the SP 800-78-5 key policy. */
#ifndef TC_ENABLE_PIV_KEY_PROOF
#define TC_ENABLE_PIV_KEY_PROOF TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_KEY_PROOF != 0 && TC_ENABLE_PIV_KEY_PROOF != 1
#error "TC_ENABLE_PIV_KEY_PROOF must be 0 or 1"
#endif
#if TC_ENABLE_PIV_KEY_PROOF &&                                                                     \
    (!TC_ENABLE_PIV_COMMAND || !TC_ENABLE_KEY_CHALLENGE || !TC_ENABLE_X509)
#error "PIV key proofs require PIV card commands, key challenges and the X.509 reader"
#endif

/* Composed card check: a report of every check on a card inventory, key
 * proofs and requirement-based acceptance. */
#ifndef TC_ENABLE_PIV_CARD_CHECK
#define TC_ENABLE_PIV_CARD_CHECK TC_PROFILE_VALUE(0, 0, 0, 1)
#endif
#if TC_ENABLE_PIV_CARD_CHECK != 0 && TC_ENABLE_PIV_CARD_CHECK != 1
#error "TC_ENABLE_PIV_CARD_CHECK must be 0 or 1"
#endif
#if TC_ENABLE_PIV_CARD_CHECK &&                                                                    \
    (!TC_ENABLE_CREDENTIAL || !TC_ENABLE_PIV_CATALOG || !TC_ENABLE_X509_PATH ||                    \
     !TC_ENABLE_X509_REVOCATION || !TC_ENABLE_GZIP)
#error                                                                                             \
    "The PIV card check requires credentials, the PIV catalog, X.509 paths and revocation, and GZIP"
#endif

/* DRBG mechanism dependencies, checked after every option is defined. */
#if TC_ENABLE_DRBG
#if !TC_DRBG_ENABLE_HASH && !TC_DRBG_ENABLE_HMAC && !TC_DRBG_ENABLE_CTR
#error "DRBG requires at least one mechanism"
#endif
#if TC_DRBG_ENABLE_HASH && !TC_ENABLE_SHA1 && !TC_ENABLE_SHA224 && !TC_ENABLE_SHA256 &&            \
    !TC_ENABLE_SHA384 && !TC_ENABLE_SHA512
#error "Hash_DRBG requires a SHA algorithm"
#endif
#if TC_DRBG_ENABLE_HMAC && !TC_ENABLE_HMAC
#error "HMAC_DRBG requires TC_ENABLE_HMAC"
#endif
#if TC_DRBG_ENABLE_CTR && (!TC_ENABLE_AES || !TC_AES_ENABLE_DYNAMIC)
#error "CTR_DRBG requires TC_ENABLE_AES and TC_AES_ENABLE_DYNAMIC"
#endif
#endif

/* Reject a consumer definition that differs from the installed archive. The
 * check runs last, after every symbolic value above is defined. */
#define TC_BUILD_CONFIG_VERIFY 1
#include "build_config.h"
#undef TC_BUILD_CONFIG_VERIFY

#endif
