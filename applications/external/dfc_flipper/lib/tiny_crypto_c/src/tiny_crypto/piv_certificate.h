/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_CERTIFICATE_H_
#define TINY_CRYPTO_PIV_CERTIFICATE_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  TC_PIV_CERTIFICATE_SLOT, TC_PIV_CERTIFICATE_TWIC, TC_PIV_CERTIFICATE_SM_SIGNER
} TC_PIV_certificate_profile;
typedef enum { TC_PIV_CERTIFICATE_PLAIN, TC_PIV_CERTIFICATE_GZIP } TC_PIV_certificate_compression;
typedef struct {
  TC_bytes certificate, intermediate_cvc;
  TC_PIV_certificate_compression compression;
} TC_PIV_certificate;

/* Read a complete tag-53 certificate container. Requires X.509 support.
 * Certificate bytes and the optional complete 7F21 CVC borrow input storage.
 * Bounds follow SP 800-73-5: certificate <=1856 bytes, intermediate CVC value
 * <=601 bytes. PIV requires empty FE; the TWIC profile ends after CertInfo.
 * This checks container fields. Decompress GZIP before parsing X.509; parse
 * and authenticate each certificate separately. out changes only on OK and
 * must be disjoint from input. Keep input unchanged while using returned spans. */
TC_TLV_result TC_PIV_certificate_read(TC_bytes input,
    TC_PIV_certificate_profile profile, TC_PIV_certificate* out);
#ifdef __cplusplus
}
#endif
#endif
