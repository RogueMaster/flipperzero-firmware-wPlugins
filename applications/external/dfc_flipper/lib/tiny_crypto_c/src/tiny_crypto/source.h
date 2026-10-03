/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Random-access byte sources for CRL preparation: a read callback over
 * files or flash.
 * Configuration: TC_ENABLE_X509_REVOCATION.
 * Contracts: docs/api.md. Guide: docs/x509-crl.md. */
#ifndef TINY_CRYPTO_SOURCE_H_
#define TINY_CRYPTO_SOURCE_H_

#include <tiny_crypto/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read callback. Copy exactly length bytes starting at offset into
 * destination and return TC_OK. The library requests only ranges inside
 * [0, TC_source.length) and never a zero length. Return TC_ERROR for a short
 * read or a storage failure. The consuming call then returns TC_TLV_IO.
 * Offsets are 64-bit, so a source may exceed the CPU address space.
 * destination is library-owned window storage, valid only for the call. */
typedef TC_status (*TC_source_read_fn)(void* context, uint64_t offset, uint8_t* destination,
                                       size_t length);

/* A random-access byte source. read and length are required. context is
 * passed to read unchanged and belongs to the caller. Keep the storage alive
 * and its bytes unchanged throughout an operation, including pauses between
 * steps. Callbacks may read files or flash and must leave library workspace
 * and window storage untouched. Publish storage updates separately while
 * existing readers keep their snapshot. */
typedef struct {
  TC_source_read_fn read;
  void* context;
  uint64_t length;
} TC_source;

#ifdef __cplusplus
}
#endif
#endif
