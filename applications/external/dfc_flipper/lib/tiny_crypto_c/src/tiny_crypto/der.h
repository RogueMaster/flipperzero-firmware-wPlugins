/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* DER value readers: INTEGER, BIT STRING, OBJECT IDENTIFIER, BOOLEAN, NULL,
 * SEQUENCE, SET and the AlgorithmIdentifier, SubjectPublicKeyInfo, PKCS #1,
 * PKCS #8 and ECDSA-Sig-Value structures.
 * Standards: ITU-T X.690 (02/2021) clauses 8 and 10-11, RFC 5280, RFC 8017,
 * RFC 5958, RFC 3279.
 * Configuration: TC_ENABLE_DER, which requires TC_ENABLE_TLV.
 * Limitations: readers check encodings. Schema, key and signature
 * validation are separate steps. No encoders.
 * Contracts: docs/api.md. Guide: docs/der.md. */
#ifndef TINY_CRYPTO_DER_H_
#define TINY_CRYPTO_DER_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Readers take the complete DER encoding, including tag and length, as a
 * borrowed span and return spans that borrow it. The input must stay
 * unchanged while a returned span is used. Output objects must be disjoint
 * from the input and from each other, since writing them would change the
 * borrowed bytes. The
 * *_contents forms take only the contents octets of an IMPLICIT value. The
 * readers charge no work and need no workspace. Every reader returns one of:
 *
 *   OK           the encoding is valid and outputs are written.
 *   INVALID      malformed, truncated or trailing input, a wrong tag or a
 *                broken DER rule, including a tag or length field wider than
 *                this build parses.
 *   LIMIT        a value exceeds the output type: TC_DER_uint32 and
 *                TC_DER_uint32_contents above UINT32_MAX only.
 *   UNSUPPORTED  a defined version outside this reader, for
 *                TC_DER_private_key_info and TC_DER_rsa_private only.
 *   ARGUMENT     a NULL output, an output that overlaps the input or
 *                another output, or a span with NULL data and a nonzero
 *                length.
 *
 * Outputs are unchanged on every failure. END and MORE are never returned.
 * The guide lists the X.690 clause behind each rule. */

#if TC_ENABLE_DER
/* INTEGER as its two's-complement contents (X.690 sections 8.3.2 and 10.1).
 * A needed sign octet stays in the span, so signed data stays byte-exact.
 * *negative is 1 for a negative value, otherwise 0. */
TC_TLV_result TC_DER_integer(TC_bytes encoded, TC_bytes* twos_complement, int* negative);
/* Strictly positive INTEGER as a borrowed unsigned magnitude without the sign
 * octet. Zero and negative values return INVALID. */
TC_TLV_result TC_DER_positive_integer(TC_bytes encoded, TC_bytes* magnitude);
/* Check IMPLICIT INTEGER contents (X.690 section 8.3.2) of any width or sign.
 * Empty contents and a redundant leading octet return INVALID. */
TC_TLV_result TC_DER_integer_contents(TC_bytes contents);
/* Nonnegative INTEGER that fits a uint32_t. Negative values return INVALID and
 * values above UINT32_MAX return LIMIT. */
TC_TLV_result TC_DER_uint32(TC_bytes encoded, uint32_t* out);
/* Contents-only form of TC_DER_uint32 for an IMPLICIT-tagged INTEGER, with
 * the same minimality, sign and LIMIT rules. */
TC_TLV_result TC_DER_uint32_contents(TC_bytes contents, uint32_t* out);
/* BIT STRING payload after the initial octet, and its unused-bit count. The
 * count is 0..7, 0 for an empty payload, and the unused bits must be zero
 * (X.690 sections 8.6.2 and 11.2). Both outputs change together. */
TC_TLV_result TC_DER_bit_string(TC_bytes encoded, TC_bytes* bits, unsigned* unused);
/* OBJECT IDENTIFIER contents with minimal base-128 subidentifiers (X.690
 * section 8.19.2). The contents remain encoded, so arcs of any size need no
 * integer conversion. Compare OIDs as byte spans. */
TC_TLV_result TC_DER_oid(TC_bytes encoded, TC_bytes* oid);
/* Contents-only form of TC_DER_oid for an IMPLICIT-tagged OBJECT IDENTIFIER.
 * Empty contents return INVALID. */
TC_TLV_result TC_DER_oid_contents(TC_bytes contents);
/* BOOLEAN with one contents octet, 00 or FF (X.690 section 11.1). *out is 0
 * or 1. */
TC_TLV_result TC_DER_boolean(TC_bytes encoded, int* out);
/* NULL with empty contents (X.690 section 8.8.2). Has no output, so ARGUMENT
 * reports only a NULL span with a length. */
TC_TLV_result TC_DER_null(TC_bytes encoded);
/* SEQUENCE (tag 30) or SET (tag 31) contents octets, unparsed. The caller
 * reads the members and checks SET OF ordering (X.690 section 11.6) and
 * DEFAULT rules (section 11.5). */
TC_TLV_result TC_DER_sequence(TC_bytes encoded, TC_bytes* contents);
TC_TLV_result TC_DER_set(TC_bytes encoded, TC_bytes* contents);
#endif

typedef struct {
  TC_bytes oid;
  /* Complete parameter encoding, or {NULL, 0} when absent. */
  TC_bytes parameters;
} TC_DER_algorithm;
#if TC_ENABLE_DER
/* AlgorithmIdentifier (RFC 5280 section 4.1.1.2): an OID and at most one
 * parameters element, returned with its tag and length. Parameter rules
 * depend on the OID and are checked by the caller. */
TC_TLV_result TC_DER_algorithm_identifier(TC_bytes encoded, TC_DER_algorithm* out);
#endif
typedef struct {
  TC_DER_algorithm algorithm;
  TC_bytes key;
} TC_DER_public_key;
#if TC_ENABLE_DER
/* SubjectPublicKeyInfo (RFC 5280 section 4.1.2.7). key is the BIT STRING
 * payload, which must be nonempty with no unused bits. */
TC_TLV_result TC_DER_subject_public_key(TC_bytes encoded, TC_DER_public_key* out);
#endif

typedef struct {
  TC_DER_algorithm algorithm;
  TC_bytes key;
  /* IMPLICIT SET OF contents, or {NULL, 0} when absent. */
  TC_bytes attributes;
  /* BIT STRING payload, or {NULL, 0} when absent. */
  TC_bytes public_key;
  unsigned public_key_unused;
} TC_DER_private_key;
#if TC_ENABLE_DER
/* PKCS #8 PrivateKeyInfo or OneAsymmetricKey (RFC 5958 section 2). Version 0
 * must omit publicKey and version 1 must carry it, otherwise the result is
 * INVALID. Later versions return UNSUPPORTED. key is the privateKey OCTET
 * STRING contents. All spans borrow the input, which holds secret key
 * material. Callers validate algorithm parameters, key contents,
 * public/private consistency and attribute schemas. */
TC_TLV_result TC_DER_private_key_info(TC_bytes encoded, TC_DER_private_key* out);
#endif

typedef struct {
  TC_bytes r, s;
} TC_DER_signature_pair;
typedef struct {
  TC_bytes modulus, exponent;
} TC_DER_rsa_public_key;
#if TC_ENABLE_DER
/* PKCS #1 RSAPublicKey (RFC 8017 appendix A.1.1) as positive, borrowed
 * magnitudes. Zero or negative components return INVALID. Validate modulus
 * and exponent constraints before cryptographic use. */
TC_TLV_result TC_DER_rsa_public(TC_bytes encoded, TC_DER_rsa_public_key* out);
#endif
typedef struct {
  TC_bytes modulus, public_exponent, private_exponent;
  TC_bytes prime1, prime2, exponent1, exponent2, coefficient;
} TC_DER_rsa_private_key;
#if TC_ENABLE_DER
/* PKCS #1 two-prime RSAPrivateKey (RFC 8017 appendix A.1.2) as eight
 * positive, borrowed magnitudes. Version 1 (multi-prime) returns
 * UNSUPPORTED. Any other version, a zero or negative component or
 * otherPrimeInfos under version 0 returns INVALID. Validate key mathematics
 * with TC_RSA_validate_private_key before use. */
TC_TLV_result TC_DER_rsa_private(TC_bytes encoded, TC_DER_rsa_private_key* out);
/* ECDSA-Sig-Value (RFC 3279 section 2.2.3) as positive magnitudes with any
 * sign octet removed. Zero or negative r or s returns INVALID. The caller
 * checks r and s against the signing key's group order. */
TC_TLV_result TC_DER_ecdsa_signature(TC_bytes encoded, TC_DER_signature_pair* out);
#endif
#ifdef __cplusplus
}
#endif
#endif
