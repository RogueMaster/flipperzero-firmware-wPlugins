/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SOURCE_INTERNAL_H_
#define TINY_CRYPTO_SOURCE_INTERNAL_H_

#include <tiny_crypto/source.h>

typedef struct {
  TC_source source;
  TC_buffer window;
  uint64_t offset, bytes_remaining, reads_remaining;
  size_t available;
} tc_source_reader;

/* Memory adapter context points to a stable TC_bytes descriptor. */
TC_status tc_source_memory_read(void* context, uint64_t offset, uint8_t* destination,
                                size_t length);

/* All storage is disjoint. Budgets count physical reads, including read-ahead and
 * failed callbacks. A zero budget permits cache hits only. */
TC_result tc_source_reader_init(tc_source_reader* reader, const TC_source* source, TC_buffer window,
                                uint64_t max_bytes, uint64_t max_reads);

/* Borrow up to length bytes at offset. The span lasts until the next cache miss.
 * reader was set up by tc_source_reader_init, which validated its storage, and
 * is private afterward. The caller supplies a non-NULL out that is disjoint
 * from reader and its window. Only the requested range is checked here.
 * ARGUMENT reports a range beyond source.length. Range and budget failures
 * preserve out. Storage failures also wipe the cache.
 * Zero length permits offset == source.length and returns an empty span. */
TC_result tc_source_reader_view(tc_source_reader* reader, uint64_t offset, size_t length,
                                TC_bytes* out);

#endif
