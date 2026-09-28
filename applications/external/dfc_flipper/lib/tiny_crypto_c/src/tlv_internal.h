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
TC_TLV_result tc_tlv_header_read(const uint8_t* data, size_t length,
    TC_TLV_profile profile, size_t length_octets, uint64_t max_value,
    tc_tlv_wide_header* out);
TC_TLV_result tc_tlv_config(TC_TLV_profile profile, const TC_TLV_limits* limits);
int tc_tlv_padding(TC_TLV_profile profile, uint8_t byte);
#endif
