/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"
#include <string.h>

#if TC_ENABLE_EC
static TC_EC_curve curve;
static TC_EC_workspace workspace;
static uint8_t scalar[48], peer[97], output[97];
static size_t width;

static TC_status public_key(size_t unused)
{
  TC_status status;
  (void)unused;
  TC_work_budget work = {UINT32_MAX};
  status = TC_EC_public_key(curve, (TC_bytes){scalar, width}, (TC_buffer){output, 2 * width + 1},
                            &workspace, &work) == TC_EC_OK
               ? TC_OK
               : TC_ERROR;
  tc_benchmark_consume(output);
  return status;
}

static TC_status validate(size_t unused)
{
  TC_status status;
  (void)unused;
  TC_work_budget work = {UINT32_MAX};
  status = TC_EC_validate_public_key(curve, (TC_bytes){peer, 2 * width + 1}, &workspace, &work) ==
                   TC_EC_OK
               ? TC_OK
               : TC_ERROR;
  tc_benchmark_consume(&workspace);
  return status;
}

static TC_status shared_secret(size_t unused)
{
  TC_status status;
  (void)unused;
  TC_work_budget work = {UINT32_MAX};
  status = TC_ECDH(curve, (TC_bytes){scalar, width}, (TC_bytes){peer, 2 * width + 1},
                   (TC_buffer){output, width}, &workspace, &work) == TC_EC_OK
               ? TC_OK
               : TC_ERROR;
  tc_benchmark_consume(output);
  return status;
}

static int measure(TC_EC_curve selected, size_t bytes)
{
  curve = selected;
  width = bytes;
  memset(scalar, 0x42, width);
  TC_work_budget work = {UINT32_MAX};
  if (TC_EC_public_key(curve, (TC_bytes){scalar, width}, (TC_buffer){peer, 2 * width + 1},
                       &workspace, &work) != TC_EC_OK)
    return 1;
  printf("EC curve=P-%lu small=%d workspace=%lu bytes\n", (unsigned long)(8 * width), TC_EC_SMALL,
         (unsigned long)sizeof workspace);
  return tc_benchmark_run("EC public key", width, public_key) ||
         tc_benchmark_run("EC point validation", 2 * width + 1, validate) ||
         tc_benchmark_run("ECDH shared secret", width, shared_secret);
}
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_EC
#if TC_EC_ENABLE_P256
  if (measure(TC_EC_P256, 32))
    return 1;
#endif
#if TC_EC_ENABLE_P384
  if (measure(TC_EC_P384, 48))
    return 1;
#endif
#else
  puts("EC disabled");
#endif
  return 0;
}
