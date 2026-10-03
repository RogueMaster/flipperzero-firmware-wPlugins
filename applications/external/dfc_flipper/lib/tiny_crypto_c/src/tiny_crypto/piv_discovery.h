/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV and TWIC Discovery Object reader: application AID and PIN usage
 * policy.
 * Standards: SP 800-73-5 Part 1 section 3.3.2 and Tables 1 and 19, TWIC
 * Part 2 v5 sections 4.2 and 4.7.5.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Limitations: structure only. The Security Object, or an object read under
 * secure messaging, supplies integrity.
 * Contracts: docs/api.md. Guide: docs/piv-card.md. */
#ifndef TINY_CRYPTO_PIV_DISCOVERY_H_
#define TINY_CRYPTO_PIV_DISCOVERY_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

/* The rule set for the policy bytes. PIV follows SP 800-73-5 Part 1. TWIC
 * reads the Discovery Object of either application on a TWIC card. */
typedef enum { TC_PIV_DISCOVERY_PIV, TC_PIV_DISCOVERY_TWIC } TC_PIV_discovery_profile;

/* Bits of the first PIN usage policy byte (Part 1 section 3.3.2). */
enum {
  TC_PIV_POLICY_PIV_PIN = 0x40,            /* bit 7: the PIV PIN satisfies the ACRs */
  TC_PIV_POLICY_GLOBAL_PIN = 0x20,         /* bit 6: the Global PIN satisfies them */
  TC_PIV_POLICY_OCC = 0x10,                /* bit 5: on-card comparison satisfies them */
  TC_PIV_POLICY_VCI = 0x08,                /* bit 4: the VCI is implemented */
  TC_PIV_POLICY_VCI_WITHOUT_PAIRING = 0x04 /* bit 3: the VCI needs no pairing code */
};

/* Second policy byte values when the Global PIN is enabled. */
enum { TC_PIV_PREFERENCE_PIV_PIN = 0x10, TC_PIV_PREFERENCE_GLOBAL_PIN = 0x20 };

typedef struct {
  TC_bytes aid;       /* the 11-byte 4F value, borrowed */
  uint8_t policy;     /* first PIN usage policy byte */
  uint8_t preference; /* second PIN usage policy byte */
  uint8_t profile;    /* the TC_PIV_discovery_profile the object was read under */
  uint8_t secured;    /* 1 when TC_PIV_discovery_get read it under secure
                         messaging (piv_vci.h), 0 from TC_PIV_discovery_read */
} TC_PIV_discovery;

#if TC_ENABLE_PIV_OBJECTS
/* Read a complete Discovery Object, 7E 12 {4F 0B AID} {5F2F 02 policy
 * preference}, as GET DATA returns it with its own tag (Part 1 Table 19).
 * The encoding must be exactly these 20 bytes.
 * - PIV: the AID is the PIV AID A0 00 00 03 08 00 00 10 00 01 00 (Part 1
 *   section 2.2). The first byte is a Table 1 value, so bits 8, 2 and 1 are
 *   zero and bit 3 needs bit 4. The second byte is 10 or 20 when bit 6 is
 *   set and 00 otherwise (section 3.3.2).
 * - TWIC: the AID is the PIV AID or a TWIC Legacy or NEXGEN AID (TWIC Part 2
 *   v5 section 4.1), and the policy is 40 00 (section 4.2), 04 00 (the PIV
 *   application, section 4.7.5) or 00 00 (the TWIC application, section
 *   4.7.5).
 * aid borrows encoded. Keep encoded unchanged while using it. encoded and out
 * must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out, NULL data with a
 * length, an unknown profile or overlap. MORE when encoded ends inside the 7E
 * object. INVALID for another tag or length, a different layout, trailing
 * bytes, or an AID or policy outside the profile. out changes only on OK.
 * out->secured is 0. Integrity comes from the Security Object (Part 1
 * section 3.3.2) or from reading the object under secure messaging with
 * TC_PIV_discovery_get. */
TC_TLV_result TC_PIV_discovery_read(TC_bytes encoded, TC_PIV_discovery_profile profile,
                                    TC_PIV_discovery* out);

/* The PIN key reference for VERIFY (Part 2 section 3.2.1, Part 1 Table 4):
 * 00 for the Global PIN when policy bit 6 is set and the preference byte is
 * 20, otherwise 80 for the PIV PIN. A NULL discovery returns 80. */
uint8_t TC_PIV_discovery_pin_reference(const TC_PIV_discovery* discovery);
#endif

#ifdef __cplusplus
}
#endif
#endif
