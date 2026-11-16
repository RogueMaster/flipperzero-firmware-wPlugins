/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <tiny_crypto/common.h>

void TC_secure_zero(void* memory, size_t length)
{
  volatile uint8_t* bytes = (volatile uint8_t*)memory;
  size_t i;

  if (memory == NULL)
    return;
  for (i = 0; i < length; ++i)
    bytes[i] = 0;
#if defined(__GNUC__) || defined(__clang__)
  /* Keep the volatile writes ordered across the call boundary. */
  __asm__ __volatile__("" ::: "memory");
#endif
}

TC_status TC_ct_equal(TC_bytes a, TC_bytes b)
{
  /* Volatile keeps the full scan visible to the compiler. The loop count may
   * reveal length, which is part of this API's public contract. */
  volatile uint8_t difference = (uint8_t)(a.length != b.length);
  size_t i;
  const size_t length = a.length < b.length ? a.length : b.length;

  if ((a.length != 0 && a.data == NULL) || (b.length != 0 && b.data == NULL))
    return TC_ERROR;
  for (i = 0; i < length; ++i)
    difference = (uint8_t)(difference | (uint8_t)(a.data[i] ^ b.data[i]));
  return difference == 0 ? TC_OK : TC_MISMATCH;
}
