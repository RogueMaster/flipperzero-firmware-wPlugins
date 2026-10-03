/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"
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
  status = TC_PIV_SM_begin(&session, suite, host_id, (TC_random_source){fixture_random, NULL},
                           &handshake, &workspace);
  tc_benchmark_consume(handshake.public_key.data);
  TC_PIV_SM_clear(&session);
  return status;
}

static TC_status wrap(size_t length)
{
  uint8_t header[16] = {0x0c, 0x87, 0x11, 0x9a, 0x80}, tag[8];
  TC_bytes authenticated[] = {{header, sizeof header}, {output, sizeof output}};
  TC_PIV_SM_protect_request request = {{input, length}, output, sizeof output, authenticated, 2};
  TC_status status;
  size_t written;
  /* A fresh synthetic session per iteration keeps transport out of the timing. */
  memset(&session, 0, sizeof session);
  session.suite = (uint8_t)suite;
  session.state = TC_PIV_SM_READY;
  session.data.traffic.counter[15] = 1;
  if (TC_PIV_SM_ciphertext_size(length, &written) != TC_OK)
    return TC_ERROR;
  authenticated[1].length = written;
  request.ciphertext_capacity = written;
  status = TC_PIV_SM_protect(&session, &request, &written, tag, &workspace);
  tc_benchmark_consume(output);
  TC_PIV_SM_clear(&session);
  return status;
}

#if TC_TEST_SM_FIXTURES && TC_ENABLE_PIV_SM_APDU
/* SD 33 card 2 application property template. The suite byte is at 40. */
static uint8_t template_answer[] = {
    0x61, 0x2a, 0x4f, 0x0b, 0xa0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x10, 0x00, 0x01,
    0x00, 0x79, 0x07, 0x4f, 0x05, 0xa0, 0x00, 0x00, 0x03, 0x08, 0x50, 0x0a, 0x49, 0x44,
    0x2d, 0x4f, 0x6e, 0x65, 0x20, 0x50, 0x49, 0x56, 0xac, 0x06, 0x80, 0x01, 0x27, 0x06,
    0x01, 0x00, 0x7f, 0x66, 0x08, 0x02, 0x02, 0x03, 0xf8, 0x02, 0x02, 0x7f, 0xff};

/* A card answering SELECT, key establishment and the protected VERIFY
 * query from the fixture. */
static TC_status fixture_transmit(void* context, TC_bytes command, TC_buffer response,
                                  size_t* length)
{
  const struct tc_sm_fixture* fixture = context;
  const TC_bytes answer = command.data[1] == 0xa4
                              ? (TC_bytes){template_answer, sizeof template_answer}
                          : command.data[1] == 0x87 ? fixture->response
                                                    : fixture->reply;
  if (answer.length + 2 > response.capacity)
    return TC_ERROR;
  memcpy(response.data, answer.data, answer.length);
  response.data[answer.length] = 0x90;
  response.data[answer.length + 1] = 0x00;
  *length = answer.length + 2;
  return TC_OK;
}

/* One session over the library link: SELECT, key establishment, key
 * confirmation and one protected command. */
static TC_status handshake_exchange(size_t unused)
{
  const struct tc_sm_fixture* fixture = &sm_fixtures[suite == TC_PIV_SM_CS2 ? 0 : 2];
  const TC_PIV_link_options options = {{TC_APDU_SHORT, 0, 8, 0, 0}, TC_PIV_CONTACT, 0};
  const uint8_t host[8] = {0};
  static uint8_t scratch[TC_APDU_SHORT_COMMAND_MAX_BYTES], sm_scratch[128],
      response[TC_PIV_SM_KEY_RESPONSE_BYTES];
  TC_PIV_link link;
  TC_PIV_application application;
  TC_PIV_SM_peer peer;
  TC_PIV_reference_status reference;
  TC_status status = TC_ERROR;
  (void)unused;
  template_answer[40] = (uint8_t)suite;
  if (TC_PIV_link_init(&link, (TC_APDU_transport){fixture_transmit, (void*)fixture}, &options,
                       (TC_buffer){scratch, sizeof scratch}) != TC_PIV_OK)
    return TC_ERROR;
  if (TC_PIV_select(&link, TC_PIV_APPLICATION_PIV, 0, (TC_buffer){response, sizeof response},
                    &application) == TC_PIV_OK &&
      TC_PIV_SM_key_request(&link, &session, suite, host, (TC_random_source){fixture_random, NULL},
                            (TC_buffer){response, sizeof response}, &peer,
                            &workspace) == TC_PIV_OK &&
      TC_PIV_SM_finish(&session, &peer, fixture->public_key, &workspace) == TC_OK &&
      TC_PIV_link_secure(&link, &workspace, (TC_buffer){sm_scratch, sizeof sm_scratch}) ==
          TC_PIV_OK &&
      TC_PIV_verify_status(&link, 0x80, &reference) == TC_PIV_OK && reference.verified)
    status = TC_OK;
  tc_benchmark_consume(&session);
  TC_PIV_link_clear(&link);
  return status;
}
#endif

static int measure(TC_PIV_SM_suite selected)
{
  suite = selected;
  printf("PIV SM suite=CS%d session=%lu workspace=%lu bytes\n", suite == TC_PIV_SM_CS2 ? 2 : 7,
         (unsigned long)sizeof session, (unsigned long)sizeof workspace);
#if TC_TEST_SM_FIXTURES && TC_ENABLE_PIV_SM_APDU
  if (tc_benchmark_run("SM handshake and exchange (fixed benchmark RNG)", 0, handshake_exchange))
    return 1;
#else
  puts("SM full-handshake fixture unavailable; configure with Python 3");
#endif
  return tc_benchmark_run("SM begin (fixed benchmark RNG)", 0, begin) ||
         tc_benchmark_run("SM wrap", 32, wrap) || tc_benchmark_run("SM wrap", sizeof input, wrap);
}
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_PIV_SM
#if TC_PIV_SM_ENABLE_CS2
  if (measure(TC_PIV_SM_CS2))
    return 1;
#endif
#if TC_PIV_SM_ENABLE_CS7
  if (measure(TC_PIV_SM_CS7))
    return 1;
#endif
#else
  puts("PIV secure messaging disabled");
#endif
  return 0;
}
