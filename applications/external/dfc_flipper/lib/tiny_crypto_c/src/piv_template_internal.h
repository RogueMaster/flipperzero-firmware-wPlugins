/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Dynamic authentication template 7C of GENERAL AUTHENTICATE (SP 800-73-5
 * Part 2 3.2.4 Table 7): witness 80, challenge 81, response 82 and
 * exponentiation 85, each a one-byte tag. Shared by key establishment and key
 * proofs. */
#ifndef TC_PIV_TEMPLATE_INTERNAL_H_
#define TC_PIV_TEMPLATE_INTERNAL_H_
#include <tiny_crypto/tlv.h>

enum {
  TC_PIV_TEMPLATE_TAG = 0x7c,
  TC_PIV_TEMPLATE_WITNESS = 0x80,
  TC_PIV_TEMPLATE_CHALLENGE = 0x81,
  TC_PIV_TEMPLATE_RESPONSE = 0x82,
  TC_PIV_TEMPLATE_EXPONENTIATION = 0x85
};

/* The template DOs one command or answer holds at most. */
#define TC_PIV_TEMPLATE_MAX_ITEMS 4u
/* The spans one DO value joins at most: CB_H, ID_sH and Q_eH of the key
 * establishment challenge (Part 2 4.1.8). */
#define TC_PIV_TEMPLATE_MAX_PARTS 3u

/* One DO of the template. Its value is the parts in order. Unused parts are
 * empty, and an empty value encodes a request, such as 82 00. */
typedef struct {
  uint8_t tag;
  TC_bytes parts[TC_PIV_TEMPLATE_MAX_PARTS];
} tc_piv_template_item;

/* Encoded size of 7C holding the items in order, or 0 when a length exceeds
 * the three-octet bound of the TLV writer. */
size_t tc_piv_template_size(const tc_piv_template_item* items, size_t count);
/* Write the template. out holds tc_piv_template_size bytes and is disjoint
 * from every item part. Returns the byte after the template. */
uint8_t* tc_piv_template_write(uint8_t* out, const tc_piv_template_item* items, size_t count);
/* Read exactly one 7C spanning encoded whose DOs are the count tags in order,
 * each once and nothing else. values[i] borrows the value of tags[i] and is
 * written only on TC_TLV_OK. ARGUMENT for count above
 * TC_PIV_TEMPLATE_MAX_ITEMS, INVALID for other framing, LIMIT for a length
 * above what size_t holds. */
TC_TLV_result tc_piv_template_read(TC_bytes encoded, const uint8_t* tags, size_t count,
                                   TC_bytes* values);
#endif
