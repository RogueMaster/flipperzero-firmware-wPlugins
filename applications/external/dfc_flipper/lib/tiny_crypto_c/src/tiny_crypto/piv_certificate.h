/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV certificate container reader: certificate, optional secure-messaging
 * intermediate CVC and MSCUID, with optional GZIP compression, and a decoder
 * that returns the DER certificate.
 * Standards: SP 800-73-5 Part 1, RFC 1952.
 * Configuration: TC_ENABLE_PIV_OBJECTS. The decoder also needs
 * TC_ENABLE_GZIP.
 * Limitations: the MSCUID lies outside the signed certificate.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_CERTIFICATE_H_
#define TINY_CRYPTO_PIV_CERTIFICATE_H_
#include <tiny_crypto/tlv.h>
#if TC_ENABLE_GZIP
#include <tiny_crypto/gzip.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  TC_PIV_CERTIFICATE_SLOT,
  TC_PIV_CERTIFICATE_TWIC,
  TC_PIV_CERTIFICATE_SM_SIGNER
} TC_PIV_certificate_profile;
typedef enum { TC_PIV_CERTIFICATE_PLAIN, TC_PIV_CERTIFICATE_GZIP } TC_PIV_certificate_compression;
typedef struct {
  /* Certificate value, the complete optional 7F21 CVC and the optional MSCUID
   * value. Absent fields have a NULL pointer. */
  TC_bytes certificate, intermediate_cvc, mscuid;
  TC_PIV_certificate_compression compression;
} TC_PIV_certificate;

/* Recommended certificate value size from SP 800-73-5 Part 1 Tables 11,
 * 16-18 and 21-40. Their footnotes allow larger certificates. */
#define TC_PIV_CERTIFICATE_RECOMMENDED_BYTES 1856

#if TC_ENABLE_PIV_OBJECTS
/* Read a complete tag-53 certificate container. Requires X.509 support.
 * Field order follows SP 800-73-5 Part 1:
 * - SLOT: 70, 71, optional MSCUID (72, 1..38 bytes), empty FE. SP 800-73-4
 *   Part 1 Tables 10, 15-17 and 20-39 deprecated MSCUID. SP 800-73-5 Tables
 *   21-40 retain it for historic retired key-management certificates. The
 *   MSCUID lies outside the signed certificate and is unauthenticated.
 * - SM_SIGNER: 70, 71, optional 7F21 intermediate CVC (value <=601 bytes),
 *   empty FE (Table 43).
 * - TWIC: 70, 71 and an optional empty FE (TWIC Part 2 v5 4.7.1, which a
 *   NEXGEN card ends with the PIV form's FE).
 * max_certificate_bytes bounds the encoded certificate value, compressed or
 * plain. Pass TC_PIV_CERTIFICATE_RECOMMENDED_BYTES unless the application
 * accepts larger certificates. Decompress GZIP before parsing X.509. This
 * checks container fields only. Parse and authenticate each certificate
 * separately.
 * Returned spans borrow input. Keep input unchanged while using them. Charges
 * no work. Returns OK, INVALID for a truncated or malformed container, LIMIT
 * when the certificate exceeds max_certificate_bytes or the CVC exceeds 601
 * bytes, and ARGUMENT for a NULL out, NULL data with a length, zero
 * max_certificate_bytes, an unknown profile, or out overlapping input. out
 * changes only on OK. */
TC_TLV_result TC_PIV_certificate_read(TC_bytes input, TC_PIV_certificate_profile profile,
                                      size_t max_certificate_bytes, TC_PIV_certificate* out);
#endif

#if TC_ENABLE_PIV_OBJECTS && TC_ENABLE_GZIP
/* Read a certificate container with TC_PIV_certificate_read and return the
 * DER certificate in out->certificate. A PLAIN certificate borrows container
 * and leaves der unused. A GZIP certificate (CertInfo 71 01 01) is decoded
 * with TC_GZIP_decode into der, and out->certificate points at der.data. In
 * both cases the certificate is exactly one DER SEQUENCE. The other fields of
 * out borrow container.
 *   max_certificate_bytes  bounds the encoded 70 value, as for
 *                          TC_PIV_certificate_read.
 *   gzip, work             decoder scratch and the TC_GZIP_decode work
 *                          budget. Both are required for every container.
 *                          Only GZIP decoding charges work.
 *   der                    caller-owned output for a decoded certificate.
 * container, gzip, work, der and out must be pairwise disjoint. Keep container
 * and der unchanged while using out.
 * Returns OK with out written. ARGUMENT for NULL gzip, work or out, NULL data
 * with a length, a zero max_certificate_bytes, an unknown profile or overlap,
 * with every argument unchanged. The TC_PIV_certificate_read results for the
 * container. After decompression, INVALID for malformed GZIP data or a
 * checksum mismatch, LIMIT when der or the work budget is too small and
 * UNSUPPORTED for a method other than deflate. INVALID unless the certificate
 * is exactly one complete DER SEQUENCE. Every failure after the argument checks
 * wipes der. out changes only on OK. Parse and authenticate the certificate
 * separately. */
TC_TLV_result TC_PIV_certificate_decode(TC_bytes container, TC_PIV_certificate_profile profile,
                                        size_t max_certificate_bytes, TC_GZIP_workspace* gzip,
                                        size_t* work, TC_buffer der, TC_PIV_certificate* out);
#endif
#ifdef __cplusplus
}
#endif
#endif
