/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The PIV and TWIC application identifiers shared by SELECT, the
 * application property template reader and the Discovery Object reader. */
#ifndef TC_PIV_AID_INTERNAL_H_
#define TC_PIV_AID_INTERNAL_H_
#include <tiny_crypto/common.h>

/* A complete AID is the 9-byte prefix followed by two version bytes. */
#define TC_PIV_AID_PREFIX_BYTES 9u
#define TC_PIV_AID_BYTES (TC_PIV_AID_PREFIX_BYTES + 2u)

enum {
  TC_PIV_AID_VERSION = 0x01,            /* PIV 01 00, TWIC 01 xx */
  TC_TWIC_AID_SUBVERSION_LEGACY = 0x01, /* TWIC Part 2 v5 section 4.1 */
  TC_TWIC_AID_SUBVERSION_NEXGEN = 0x03
};

/* Row 0: the PIV AID prefix (SP 800-73-5 Part 1 section 2.2). Row 1: the TWIC
 * AID prefix (TWIC Part 2 v5 section 4.1, Appendix C). tests/twic/apdu_replay.py
 * and the capture tools mirror this table. */
extern const uint8_t tc_piv_aid_prefixes[2][TC_PIV_AID_PREFIX_BYTES];
/* The complete PIV AID with version 01 00. */
extern const uint8_t tc_piv_aid[TC_PIV_AID_BYTES];

/* 1 when aid is a complete TWIC AID with version 01 and the Legacy or NEXGEN
 * sub-version (TWIC Part 2 v5 section 4.1). */
int tc_twic_aid_known(TC_bytes aid);
#endif
