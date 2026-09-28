/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SOURCE_DER_INTERNAL_H_
#define TINY_CRYPTO_SOURCE_DER_INTERNAL_H_
#include "source_internal.h"
#include "tlv_internal.h"

typedef struct {
  tc_tlv_wide_header header;
  uint64_t offset, value_offset, end;
} tc_source_der_element;

/* Read one header within [offset,end). Validate the complete value's bounds
 * without loading its contents. Output changes only on success. */
TC_TLV_result tc_source_der_read(tc_source_reader* reader, uint64_t offset,
    uint64_t end, uint64_t max_value, tc_source_der_element* out);
#endif
