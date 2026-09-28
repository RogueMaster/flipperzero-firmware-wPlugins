/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PIV_SM_INTERNAL_H_
#define TC_PIV_SM_INTERNAL_H_
#include <tiny_crypto/piv_sm.h>
#include <tiny_crypto/sskdf.h>
#include "internal.h"

typedef struct {
  TC_EC_curve curve;
  size_t coordinate_bytes, key_bytes, nonce_bytes;
  uint8_t prefix[6];
  TC_status (*derive)(const uint8_t*, size_t, const TC_bytes*, size_t, uint8_t*, size_t);
} tc_sm_suite;

const tc_sm_suite* tc_sm_suite_get(unsigned suite);
int tc_sm_disjoint(const TC_bytes* writable, size_t count, const TC_bytes* input, size_t inputs);
TC_status tc_sm_mac(TC_PIV_SM_workspace* w, const uint8_t* key, size_t key_len,
    const TC_bytes* input, size_t count, uint8_t output[16]);

#define TC_SM_SYM(w) ((w)->operation.symmetric)
#endif
