/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The key policy check shared by the key policy and the key proof. */
#ifndef TC_PIV_KEY_PROOF_INTERNAL_H_
#define TC_PIV_KEY_PROOF_INTERNAL_H_
#include <tiny_crypto/piv_key_proof.h>

/* 1 when policy holds a known profile and padding, allow_rsa1024 of 0 or 1
 * and a valid time. */
int tc_piv_key_policy_valid(const TC_PIV_key_policy* policy);
#endif
