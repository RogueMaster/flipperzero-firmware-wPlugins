/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_HASH_STREAM_INTERNAL_H_
#define TC_HASH_STREAM_INTERNAL_H_

#include <tiny_crypto/common.h>
#include <string.h>

typedef void (*tc_hash_block_fn)(void* context, const uint8_t* block);
typedef enum { TC_HASH_LENGTH_BIG_ENDIAN, TC_HASH_LENGTH_LITTLE_ENDIAN } tc_hash_length_encoding;

/* Complete input blocks are borrowed. Only a partial block enters the context. */
static inline void tc_hash_stream_absorb(void* context, uint8_t* used, uint8_t* buffer,
                                         const uint8_t* data, size_t length, size_t block_bytes,
                                         tc_hash_block_fn compress)
{
  if (*used && length) {
    size_t available = block_bytes - *used;
    size_t take = length < available ? length : available;
    memcpy(buffer + *used, data, take);
    *used = (uint8_t)(*used + take);
    data += take;
    length -= take;
    if (*used == block_bytes) {
      compress(context, buffer);
      *used = 0;
    }
  }
  while (length >= block_bytes) {
    compress(context, data);
    data += block_bytes;
    length -= block_bytes;
  }
  if (length) {
    memcpy(buffer, data, length);
    *used = (uint8_t)length;
  }
}

/* The 128-bit length field uses the high and low halves of count * 8. */
static inline void tc_hash_stream_finish(void* context, uint64_t count, uint8_t* used,
                                         uint8_t* buffer, size_t block_bytes, size_t length_bytes,
                                         tc_hash_length_encoding encoding,
                                         tc_hash_block_fn compress)
{
  const size_t length_offset = block_bytes - length_bytes;
  const uint64_t low_bits = count << 3;
  const uint64_t high_bits = count >> 61;
  size_t i;

  buffer[(*used)++] = 0x80;
  if (*used > length_offset) {
    memset(buffer + *used, 0, block_bytes - *used);
    compress(context, buffer);
    *used = 0;
  }
  memset(buffer + *used, 0, length_offset - *used);
  for (i = 0; i < length_bytes; ++i) {
    const uint8_t octet =
        i < 8 ? (uint8_t)(low_bits >> (8u * i)) : (uint8_t)(high_bits >> (8u * (i - 8u)));
    const size_t position = encoding == TC_HASH_LENGTH_LITTLE_ENDIAN ? i : length_bytes - 1u - i;
    buffer[length_offset + position] = octet;
  }
  compress(context, buffer);
}

#endif
