/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_SOURCE_H_
#define TINY_CRYPTO_SOURCE_H_

#include <tiny_crypto/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fill the entire destination and return TC_OK. Short reads and storage failures
 * return TC_ERROR. Offsets address the source independently of CPU address size. */
typedef TC_status (*TC_source_read_fn)(void* context, uint64_t offset,
    uint8_t* destination, size_t length);

/* The caller holds storage alive and immutable throughout an operation, including
 * pauses. Callbacks may use files or flash; they must preserve parser workspace.
 * Publish storage updates separately while existing readers retain their snapshot. */
typedef struct {
  TC_source_read_fn read;
  void* context;
  uint64_t length;
} TC_source;

#ifdef __cplusplus
}
#endif
#endif
