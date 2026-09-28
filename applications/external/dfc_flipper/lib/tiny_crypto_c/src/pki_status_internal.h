/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_STATUS_INTERNAL_H_
#define TC_PKI_STATUS_INTERNAL_H_
#include <tiny_crypto/x509.h>

static inline TC_TLV_result tc_pki_signature_status(TC_X509_signature_result result)
{
  switch (result) {
    case TC_X509_SIGNATURE_VALID: return TC_TLV_OK;
    case TC_X509_SIGNATURE_INVALID: return TC_TLV_INVALID;
    case TC_X509_SIGNATURE_UNSUPPORTED: return TC_TLV_UNSUPPORTED;
    case TC_X509_SIGNATURE_LIMIT: return TC_TLV_LIMIT;
    default: return TC_TLV_ARGUMENT;
  }
}

static inline TC_X509_signature_result tc_pki_signature_error(TC_TLV_result result)
{
  switch (result) {
    case TC_TLV_LIMIT: return TC_X509_SIGNATURE_LIMIT;
    case TC_TLV_UNSUPPORTED: return TC_X509_SIGNATURE_UNSUPPORTED;
    case TC_TLV_INVALID: case TC_TLV_MORE: return TC_X509_SIGNATURE_INVALID;
    default: return TC_X509_SIGNATURE_ERROR;
  }
}
#endif
