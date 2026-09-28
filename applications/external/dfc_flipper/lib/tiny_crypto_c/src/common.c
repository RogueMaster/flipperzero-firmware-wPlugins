/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <tiny_crypto/common.h>

void TC_secure_zero(void* memory, size_t length)
{
  volatile uint8_t* bytes = (volatile uint8_t*)memory;
  size_t i;

  for (i = 0; i < length; ++i)
    bytes[i] = 0;
#if defined(__GNUC__) || defined(__clang__)
  /* Keep the volatile writes ordered across the call boundary. */
  __asm__ __volatile__("" ::: "memory");
#endif
}

TC_status TC_ct_equal(const uint8_t* a, const uint8_t* b, size_t length)
{
  /* Volatile keeps the full scan visible to the compiler. The loop count may
   * reveal length, which is part of this API's public contract. */
  volatile uint8_t difference = 0;
  size_t i;

  if (length != 0 && (a == NULL || b == NULL))
    return TC_ERROR;
  for (i = 0; i < length; ++i)
    difference = (uint8_t)(difference | (uint8_t)(a[i] ^ b[i]));
  return difference == 0 ? TC_OK : TC_MISMATCH;
}
