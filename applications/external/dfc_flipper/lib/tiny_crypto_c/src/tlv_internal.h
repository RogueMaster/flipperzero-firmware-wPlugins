/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_TLV_INTERNAL_H_
#define TC_TLV_INTERNAL_H_
#include <tiny_crypto/tlv.h>
typedef struct {
  uint64_t length;
  uint32_t number;
  uint8_t tag[TC_TLV_TAG_BYTES];
  uint8_t tag_length, header_length, tag_class, constructed, indefinite;
} tc_tlv_wide_header;

/* Shared framing checks for address-sized buffers and wide storage offsets. */
TC_TLV_result tc_tlv_header_read(const uint8_t* data, size_t length, TC_TLV_profile profile,
                                 size_t length_octets, uint64_t max_value, tc_tlv_wide_header* out);
/* Address-sized header read after the caller validated profile, limits and
 * pointers once. Returns the TC_TLV_header_read framing results. */
TC_TLV_result tc_tlv_header_parse(const uint8_t* data, size_t length, TC_TLV_profile profile,
                                  size_t max_value, TC_TLV_header* out);
TC_TLV_result tc_tlv_config(TC_TLV_profile profile, const TC_TLV_limits* limits);
int tc_tlv_padding(TC_TLV_profile profile, uint8_t byte);

/* Size of a TLV header with a tag of tag_length bytes and the minimal
 * definite length field for value_length (X.690 8.1.3, DER 10.1, ISO/IEC
 * 7816-4 6.3). The length field takes one to four octets, so value_length is
 * at most FFFFFF. Returns 0 for a tag outside 1..TC_TLV_TAG_BYTES bytes or a
 * longer value. */
size_t tc_tlv_header_size(size_t tag_length, size_t value_length);
/* Write the tag bytes and the minimal length field and return the byte after
 * the header. The caller checked tc_tlv_header_size and provides that many
 * bytes at out. tag and out must not overlap. */
uint8_t* tc_tlv_header_write(uint8_t* out, const uint8_t* tag, size_t tag_length,
                             size_t value_length);
#endif
