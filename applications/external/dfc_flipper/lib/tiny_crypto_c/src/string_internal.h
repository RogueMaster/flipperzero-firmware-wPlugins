/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_STRING_INTERNAL_H_
#define TC_STRING_INTERNAL_H_
#include <tiny_crypto/tlv.h>

/* Decode one scalar from UTF8String, PrintableString, IA5String, VisibleString,
 * UniversalString or BMPString contents. Offset and point change only on OK.
 * TeletexString needs a separate, explicitly chosen character mapping. */
TC_TLV_result tc_asn1_string_next(unsigned tag, TC_bytes input,
    size_t* offset, uint32_t* point);

enum { TC_ASN1_SCALAR_BYTES = 4 };
typedef struct {
  unsigned tag;
  uint8_t pending[TC_ASN1_SCALAR_BYTES];
  size_t used;
} tc_asn1_string_state;
typedef TC_TLV_result (*tc_asn1_string_consume)(void* context, uint32_t point);
/* Start with tag set and used zero. Only a split scalar is copied. On success,
 * used must be zero at end of input. Discard state after an error. All input,
 * state and callback storage must be disjoint. NULL consume checks syntax. */
TC_TLV_result tc_asn1_string_feed(tc_asn1_string_state* state, TC_bytes bytes,
    tc_asn1_string_consume consume, void* context);
#endif
