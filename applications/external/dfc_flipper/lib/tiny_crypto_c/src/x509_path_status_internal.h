/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_PATH_STATUS_INTERNAL_H_
#define TC_X509_PATH_STATUS_INTERNAL_H_
#include <tiny_crypto/x509_path.h>

/* Status conversions shared by path validation, search and revocation. */

/* An unevaluated branch prevents a definitive no-path result. */
static inline void tc_x509_path_remember(TC_X509_path_status status, TC_X509_path_status* failure)
{
  if (status == TC_X509_PATH_LIMIT ||
      (status == TC_X509_PATH_UNSUPPORTED && *failure != TC_X509_PATH_LIMIT))
    *failure = status;
}

static inline TC_X509_path_status tc_x509_path_status(TC_TLV_result result)
{
  switch (result) {
  case TC_TLV_OK:
    return TC_X509_PATH_VALID;
  case TC_TLV_LIMIT:
    return TC_X509_PATH_LIMIT;
  case TC_TLV_UNSUPPORTED:
    return TC_X509_PATH_UNSUPPORTED;
  case TC_TLV_ARGUMENT:
  case TC_TLV_IO: /* a storage source failed to supply bytes */
    return TC_X509_PATH_ERROR;
  default:
    return TC_X509_PATH_INVALID;
  }
}

static inline TC_TLV_result tc_x509_path_result_status(TC_X509_path_status status)
{
  switch (status) {
  case TC_X509_PATH_VALID:
    return TC_TLV_OK;
  case TC_X509_PATH_INVALID:
    return TC_TLV_INVALID;
  case TC_X509_PATH_UNSUPPORTED:
    return TC_TLV_UNSUPPORTED;
  case TC_X509_PATH_LIMIT:
    return TC_TLV_LIMIT;
  default:
    return TC_TLV_ARGUMENT;
  }
}
#endif
