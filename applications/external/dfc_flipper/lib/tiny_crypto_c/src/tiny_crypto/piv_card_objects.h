/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV card object readers: Card Capability Container, Key History, BIT
 * group template and Pairing Code container.
 * Standards: SP 800-73-5 Part 1 sections 3.1.1, 3.3.3, 3.3.6 and 3.3.8 and
 * Tables 9, 20, 42 and 44, SP 800-73-4 Part 1 Table 8.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Limitations: structure only. The Security Object supplies integrity.
 * Contracts: docs/api.md. Guide: docs/piv-card.md. */
#ifndef TINY_CRYPTO_PIV_CARD_OBJECTS_H_
#define TINY_CRYPTO_PIV_CARD_OBJECTS_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

/* CONTAINER is the complete 53 object that GET DATA returns. CONTENTS starts
 * at the first data element. */
typedef enum { TC_PIV_CONTENTS, TC_PIV_CONTAINER } TC_PIV_container_encoding;

/* Shared reader rules. Every reader checks one complete object, returns views
 * that borrow encoded, charges no work and writes out only on OK. Keep
 * encoded unchanged while using the views, and keep encoded and out
 * disjoint. Results:
 *   OK        the object matches its table and out is written.
 *   ARGUMENT  NULL out, NULL data with a length, an unknown encoding, or
 *             overlap.
 *   MORE      encoded ends inside the outer 53 or 7F61 object.
 *   INVALID   empty input, another outer tag, trailing bytes, a missing,
 *             reordered, repeated or oversized element, or a tag or length
 *             field beyond ISO/IEC 7816-4 section 6.3. */

typedef struct {
  TC_bytes card_identifier;      /* F0, empty or 21 bytes */
  TC_bytes card_url;             /* F3, 0..128 bytes */
  TC_bytes access_control_rules; /* F6, empty or 17 bytes */
  int container_version;         /* F1 value, -1 when empty */
  int grammar_version;           /* F2 value, -1 when empty */
  int pkcs15;                    /* F4 value, -1 when empty */
  uint8_t data_model;            /* F5 value. PIV cards use 10 (Part 1 3.1.1). */
} TC_PIV_CCC;

#if TC_ENABLE_PIV_OBJECTS
/* Read a Card Capability Container (Part 1 section 3.1.1, Table 9): F0, F1,
 * F2, F3, F4, F5, F6, F7, FA, FB, FC, FD and FE in this order with the
 * listed lengths. F7 and FA to FE are empty. The optional E3 and B4 elements
 * of SP 800-73-4 Part 1 Table 8 (at most 48 bytes each) are accepted in that
 * order before FE and skipped. The data model number is reported as read.
 * Results follow the shared reader rules. */
TC_TLV_result TC_PIV_CCC_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                              TC_PIV_CCC* out);
#endif

/* Part 1 section 3.3.3: at most 20 retired key management keys. */
#define TC_PIV_KEY_HISTORY_MAX_KEYS 20u
/* Part 1 Table 20: the offCardCertURL value holds at most 118 bytes. */
#define TC_PIV_KEY_HISTORY_URL_MAX_BYTES 118u

typedef struct {
  uint8_t on_card;  /* keysWithOnCardCerts */
  uint8_t off_card; /* keysWithOffCardCerts */
  TC_bytes url;     /* offCardCertURL, NULL data when absent */
} TC_PIV_key_history;

#if TC_ENABLE_PIV_OBJECTS
/* Read a Key History object (Part 1 section 3.3.3, Table 20): C1 and C2 of
 * one byte each with a sum of at most 20, an optional F3, and an empty FE.
 * F3 is required when C2 is nonzero, optional when only C1 is nonzero and
 * absent when both are zero (Table 20 footnote 25). F3 holds at most 118
 * bytes of the form "http://" <DNS name> "/" <64 hexadecimal digits>, where
 * the DNS name is dot-separated LDH labels (RFC 5890 section 2.3.1). Results
 * follow the shared reader rules. */
TC_TLV_result TC_PIV_key_history_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                      TC_PIV_key_history* out);
#endif

/* Part 1 Table 42: the BIT for one finger holds at most 28 bytes. */
#define TC_PIV_BIT_MAX_BYTES 28u

typedef struct {
  unsigned fingers;      /* 0, 1 or 2 */
  TC_bytes templates[2]; /* 7F60 values in card order, NULL data when unused */
} TC_PIV_bit_group;

#if TC_ENABLE_PIV_OBJECTS
/* Read a complete BIT group template, 7F61 {02 01 n} followed by n 7F60
 * templates, as GET DATA returns it with its own tag (Part 1 section 3.3.6,
 * Table 42). n is 0, 1 or 2, and each 7F60 value holds 1 to 28 bytes.
 * 7F 61 03 02 01 00 is the empty group (section 3.3.6 footnote 6). The BIT
 * contents are left to an on-card comparison reader. A nonempty group needs
 * the Discovery Object OCC bit (section 3.3.6), which the caller checks
 * across objects. Results follow the shared reader rules. */
TC_TLV_result TC_PIV_bit_group_read(TC_bytes encoded, TC_PIV_bit_group* out);
#endif

/* Part 1 Table 44 and Part 2 section 2.4.3: eight ASCII digits. */
#define TC_PIV_PAIRING_CODE_BYTES 8u

#if TC_ENABLE_PIV_OBJECTS
/* Read a Pairing Code Reference Data Container (Part 1 section 3.3.8,
 * Table 44): 99 with eight ASCII digits, then an empty FE. code borrows the
 * 99 value. The object is secret: it is PIN-gated on the card (Part 1
 * Table 2), and the caller wipes encoded when done. Results follow the shared
 * reader rules. */
TC_TLV_result TC_PIV_pairing_code_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                       TC_bytes* code);
#endif

#ifdef __cplusplus
}
#endif
#endif
