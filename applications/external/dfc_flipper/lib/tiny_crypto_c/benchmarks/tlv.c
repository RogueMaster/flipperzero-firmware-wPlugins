/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"
#include <string.h>

#if TC_ENABLE_TLV
static uint8_t input[4096];
static size_t input_size;
static TC_status walk(size_t unused)
{
  TC_TLV_limits limits = {sizeof input, sizeof input, 4096, 16};
  TC_TLV_frame frames[16];
  TC_TLV_result result;
  (void)unused;
  result = TC_TLV_walk(input, input_size, TC_TLV_DER, &limits, frames, 16, NULL, NULL);
  tc_benchmark_consume(&result);
  return result == TC_TLV_OK ? TC_OK : TC_ERROR;
}

static TC_status reject(size_t unused)
{
  TC_TLV_limits limits = {sizeof input, sizeof input, 4096, 16};
  TC_TLV_frame frames[16];
  TC_TLV_result result;
  (void)unused;
  result = TC_TLV_walk(input, input_size, TC_TLV_DER, &limits, frames, 16, NULL, NULL);
  tc_benchmark_consume(&result);
  return result == TC_TLV_INVALID ? TC_OK : TC_ERROR;
}
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_TLV
  {
    size_t i;
    printf("TLV reader=%lu stream=%lu frame=%lu bytes\n",
           (unsigned long)sizeof(TC_TLV_reader), (unsigned long)sizeof(TC_TLV_stream),
           (unsigned long)sizeof(TC_TLV_frame));
    for (i = 0; i < sizeof input; i += 2) { input[i] = 4; input[i+1] = 0; }
    input_size = sizeof input;
    if (tc_benchmark_run("TLV empty siblings", input_size, walk)) return 1;
    memset(input, 0, sizeof input);
    input[0] = 4; input[1] = 0x82; input[2] = 0x0f; input[3] = 0xfc;
    if (tc_benchmark_run("TLV skip primitive payload", input_size, walk)) return 1;
    for (i = 0; i < 16; ++i) { input[2*i] = 0x30; input[2*i+1] = (uint8_t)(32-2*i); }
    input[32] = 4; input[33] = 0; input_size = 34;
    if (tc_benchmark_run("TLV nested containers", input_size, walk)) return 1;
    --input_size;
    if (tc_benchmark_run("TLV truncated container", input_size, reject)) return 1;
  }
#else
  puts("TLV parsing disabled");
#endif
  return 0;
}
