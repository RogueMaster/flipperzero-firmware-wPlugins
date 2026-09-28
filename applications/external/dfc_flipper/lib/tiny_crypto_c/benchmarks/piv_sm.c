/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"
#if TC_ENABLE_PIV_CVC
#include "../examples/piv_sm_wire.h"
#endif
#include <string.h>
#if TC_TEST_SM_FIXTURES
#include "sm_fixtures.h"
#endif

#if TC_ENABLE_PIV_SM
static TC_PIV_SM_suite suite;
static TC_PIV_SM session;
static TC_PIV_SM_workspace workspace;
static uint8_t input[4096], output[4144];

static TC_status fixture_random(void* user, uint8_t* bytes, size_t length)
{
  (void)user;
  memset(bytes, 0, length);
  bytes[length - 1] = 1;
  return TC_OK;
}

static TC_status begin(size_t unused)
{
  const uint8_t host_id[8] = {0};
  TC_PIV_SM_handshake handshake;
  TC_status status;
  (void)unused;
  status = TC_PIV_SM_begin(&session,suite,host_id,fixture_random,NULL,
                           &handshake,&workspace);
  tc_benchmark_consume(handshake.public_key.data);
  TC_PIV_SM_clear(&session);
  return status;
}

static TC_status wrap(size_t length)
{
  uint8_t header[16] = {0x0c,0x87,0x11,0x9a,0x80}, tag[8];
  TC_bytes authenticated[] = {{header,sizeof header},{output,sizeof output}};
  TC_PIV_SM_protect_request request = {
    {input,length},output,sizeof output,authenticated,2
  };
  TC_status status;
  size_t written;
  /* A fresh synthetic session per iteration keeps transport out of the timing. */
  memset(&session, 0, sizeof session);
  session.suite = (uint8_t)suite;
  session.state = TC_PIV_SM_READY;
  session.data.traffic.counter[15] = 1;
  if (TC_PIV_SM_ciphertext_size(length,&written) != TC_OK) return TC_ERROR;
  authenticated[1].length = written;
  request.ciphertext_capacity = written;
  status = TC_PIV_SM_protect(&session,&request,&written,tag,&workspace);
  tc_benchmark_consume(output);
  TC_PIV_SM_clear(&session);
  return status;
}

#if TC_TEST_SM_FIXTURES && TC_ENABLE_PIV_CVC
static TC_status handshake_exchange(size_t unused)
{
  const struct tc_sm_fixture* fixture = &sm_fixtures[suite == TC_PIV_SM_CS2 ? 0 : 1];
  const uint8_t host[8] = {0};
  ExamplePIVSMCommand command = {{NULL, 0}, 0x20, 0, 0x80, 0};
  ExamplePIVSMResult result;
  size_t written;
  TC_status status;
  (void)unused;
  status = example_piv_sm_begin(&session,suite,host,fixture_random,NULL,
                           output, sizeof output, &written, &workspace);
  if (status == TC_OK)
    status = example_piv_sm_finish(&session,fixture->response,0x9000,
                                   fixture->public_key,&workspace);
  if (status == TC_OK)
    status = example_piv_sm_protect(&session,&command,output,sizeof output,&written,&workspace);
  if (status == TC_OK)
    status = example_piv_sm_unprotect(&session,fixture->reply,0x9000,
                                      output,sizeof output,&result,&workspace);
  if (status == TC_OK && (result.length != 0 || result.status != 0x9000)) status = TC_ERROR;
  tc_benchmark_consume(&session);
  TC_PIV_SM_clear(&session);
  return status;
}
#endif

static int measure(TC_PIV_SM_suite selected)
{
  suite = selected;
  printf("PIV SM suite=CS%d session=%lu workspace=%lu bytes\n",
         suite == TC_PIV_SM_CS2 ? 2 : 7, (unsigned long)sizeof session,
         (unsigned long)sizeof workspace);
#if TC_TEST_SM_FIXTURES && TC_ENABLE_PIV_CVC
  if (tc_benchmark_run("SM handshake and exchange (fixed benchmark RNG)", 0, handshake_exchange)) return 1;
#else
  puts("SM full-handshake fixture unavailable; configure with Python 3");
#endif
  return tc_benchmark_run("SM begin (fixed benchmark RNG)", 0, begin) ||
         tc_benchmark_run("SM wrap", 32, wrap) ||
         tc_benchmark_run("SM wrap", sizeof input, wrap);
}
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_PIV_SM
#if TC_PIV_SM_ENABLE_CS2
  if (measure(TC_PIV_SM_CS2)) return 1;
#endif
#if TC_PIV_SM_ENABLE_CS7
  if (measure(TC_PIV_SM_CS7)) return 1;
#endif
#else
  puts("PIV secure messaging disabled");
#endif
  return 0;
}
