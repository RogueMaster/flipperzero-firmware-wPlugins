/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_PRINTED_H_
#define TINY_CRYPTO_PIV_PRINTED_H_
#include <tiny_crypto/tlv.h>
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_PIV_PRINTED_CONTENTS,
  TC_PIV_PRINTED_CONTAINER
} TC_PIV_printed_encoding;

typedef enum {
  TC_PIV_PRINTED_PROFILE_PIV,
  TC_PIV_PRINTED_PROFILE_TWIC
} TC_PIV_printed_profile;

typedef struct {
  TC_bytes name;
  TC_bytes employee_affiliation;
  TC_bytes expiration_text;
  TC_bytes card_serial_number;
  TC_bytes issuer_identification;
  TC_bytes organization_1;
  TC_bytes organization_2;
  TC_X509_time expiration;
} TC_PIV_printed;

/* Parse a PIV or decrypted TWIC printed-information object.
 * Returned spans borrow input. out changes only on success. */
TC_TLV_result TC_PIV_printed_read(TC_bytes input,
                                  TC_PIV_printed_encoding encoding,
                                  TC_PIV_printed_profile profile,
                                  TC_PIV_printed *out);

/* Check the printed date against an authenticated CHUID and evaluation time.
 * valid receives one when both checks pass. */
TC_TLV_result TC_PIV_printed_expiration_check(const TC_PIV_printed *printed,
                                              TC_bytes chuid_expiration,
                                              const TC_X509_time *at,
                                              int *valid);

#ifdef __cplusplus
}
#endif
#endif
