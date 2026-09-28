/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_UNICODE_INTERNAL_H_
#define TC_UNICODE_INTERNAL_H_
#include <tiny_crypto/tlv.h>

/* Unicode 3.2 NFKC over scalar values. Storage must hold the decomposed form.
 * All ranges must be disjoint. Storage may change on failure; written changes
 * only on success. Work is consumed for input, output and ordering moves. */
TC_TLV_result tc_unicode_nfkc(const uint32_t* input, size_t length,
    uint32_t* storage, size_t capacity, size_t* written, size_t* work);
/* RFC 4518 mapping precedes NFKC; rejection and space handling follow it. */
size_t tc_unicode_map(uint32_t point, uint32_t mapped[4]);
int tc_unicode_allowed(uint32_t point);
int tc_unicode_mark(uint32_t point);
/* Reject prohibited normalized values and fold spaces for stored attributes.
 * Apply once: a leading combining mark can attach to the added boundary space.
 * Buffer contents may change on failure; written changes only on success. */
TC_TLV_result tc_unicode_finish_name(uint32_t* buffer, size_t length,
    size_t capacity, size_t* written, size_t* work);
/* RFC 4518 case-ignore preparation of ASN.1 string contents, using one scalar
 * buffer for decomposition and the result. Same storage/work contract as NFKC. */
TC_TLV_result tc_unicode_prepare(unsigned tag, TC_bytes input,
    uint32_t* storage, size_t capacity, size_t* written, size_t* work);
/* Incremental preparation: start used at zero, append decoded scalars, then
 * finish once for the whole attribute. Callers preflight disjoint storage.
 * Scratch and used are provisional until finish succeeds. */
TC_TLV_result tc_unicode_prepare_point(uint32_t point, uint32_t* storage,
    size_t capacity, size_t* used, size_t* work);
TC_TLV_result tc_unicode_prepare_finish(uint32_t* storage, size_t used,
    size_t capacity, size_t* written, size_t* work);
#endif
