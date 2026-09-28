/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_HASH64_INTERNAL_H_
#define TC_HASH64_INTERNAL_H_
#include <tiny_crypto/common.h>
#include <string.h>

enum { TC_HASH64_BLOCK_BYTES = 64, TC_HASH64_LENGTH_BYTES = 8,
       TC_HASH64_LENGTH_OFFSET = TC_HASH64_BLOCK_BYTES - TC_HASH64_LENGTH_BYTES };
typedef void (*tc_hash_compress_fn)(uint32_t* state, const uint8_t* block);
typedef enum { TC_HASH_LENGTH_BIG_ENDIAN, TC_HASH_LENGTH_LITTLE_ENDIAN } tc_hash_length_encoding;

/* Count handling belongs to the digest. Complete input blocks are borrowed;
 * only a partial block is copied into the context. */
static inline void tc_hash64_absorb(uint32_t* state, uint8_t* used, uint8_t* buffer,
    const uint8_t* data, size_t length, tc_hash_compress_fn compress)
{
  if (*used && length) {
    size_t available = TC_HASH64_BLOCK_BYTES - *used;
    size_t take = length < available ? length : available;
    memcpy(buffer + *used,data,take);
    *used = (uint8_t)(*used + take);
    data += take;
    length -= take;
    if (*used == TC_HASH64_BLOCK_BYTES) {
      compress(state,buffer);
      *used = 0;
    }
  }
  while (length >= TC_HASH64_BLOCK_BYTES) {
    compress(state,data);
    data += TC_HASH64_BLOCK_BYTES;
    length -= TC_HASH64_BLOCK_BYTES;
  }
  if (length) {
    memcpy(buffer,data,length);
    *used = (uint8_t)length;
  }
}

/* Append the one-bit padding and encoded bit length, then compress the tail.
 * The caller serializes the resulting chaining words into its digest. */
static inline void tc_hash64_finish(uint32_t* state, uint64_t count,
    uint8_t* used, uint8_t* buffer, tc_hash_compress_fn compress,
    tc_hash_length_encoding encoding)
{
  uint64_t bits = count << 3;
  buffer[(*used)++] = 0x80;
  if (*used > TC_HASH64_LENGTH_OFFSET) {
    memset(buffer + *used,0,TC_HASH64_BLOCK_BYTES - *used);
    compress(state,buffer);
    *used = 0;
  }
  memset(buffer + *used,0,TC_HASH64_LENGTH_OFFSET - *used);
  for (unsigned i = 0; i < TC_HASH64_LENGTH_BYTES; ++i) {
    unsigned position = encoding == TC_HASH_LENGTH_LITTLE_ENDIAN ? i : TC_HASH64_LENGTH_BYTES - 1 - i;
    buffer[TC_HASH64_LENGTH_OFFSET + position] = (uint8_t)(bits >> (8 * i));
  }
  compress(state,buffer);
}
#endif
