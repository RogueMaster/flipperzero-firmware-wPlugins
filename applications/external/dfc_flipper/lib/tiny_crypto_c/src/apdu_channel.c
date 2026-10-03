/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Bounded command-response exchange: command chaining (ISO/IEC 7816-4:2020
 * 5.3.3), response chaining with GET RESPONSE (5.3.4, 5.6), 6CXX correction
 * (5.6) and card size limits (12.8.1). */
#include <tiny_crypto/apdu.h>
#if TC_ENABLE_APDU
#include "apdu_internal.h"
#include "internal.h"

enum {
  KNOWN_FLAGS = TC_APDU_GET_RESPONSE_PLAIN_CLA,
  GET_RESPONSE = 0xc0,
  MORE_DATA = 0x61,
  WRONG_LENGTH = 0x6c,
  SUCCESS = 0x9000,
  /* The smallest card limits that hold a header, or SW1 SW2 and one byte. */
  MIN_COMMAND_BYTES = TC_APDU_HEADER_BYTES,
  MIN_RESPONSE_BYTES = TC_APDU_STATUS_BYTES + 1
};

/* Progress of one transceive over the caller's response buffer. */
typedef struct {
  TC_buffer response;
  size_t used;       /* response data bytes kept */
  size_t high_water; /* end of the bytes the transport reported */
  uint16_t sw;
  uint8_t format; /* length-field format of this exchange */
} exchange_state;

static int limits_valid(size_t max_command_bytes, size_t max_response_bytes)
{
  return (!max_command_bytes || max_command_bytes >= MIN_COMMAND_BYTES) &&
         (!max_response_bytes || max_response_bytes >= MIN_RESPONSE_BYTES);
}

TC_APDU_result TC_APDU_channel_init(TC_APDU_channel* channel, TC_APDU_transport transport,
                                    const TC_APDU_channel_options* options, TC_buffer scratch)
{
  if (!channel || !options || !transport.transmit || !scratch.data ||
      scratch.capacity < TC_APDU_HEADER_BYTES ||
      !tc_internal_ranges_disjoint(scratch.data, scratch.capacity, channel, sizeof *channel) ||
      !tc_internal_ranges_disjoint(scratch.data, scratch.capacity, options, sizeof *options) ||
      (options->format != TC_APDU_SHORT && options->format != TC_APDU_EXTENDED) ||
      (options->flags & ~(unsigned)KNOWN_FLAGS) || !options->exchanges ||
      !limits_valid(options->max_command_bytes, options->max_response_bytes))
    return TC_APDU_ARGUMENT;
  channel->transport = transport;
  channel->scratch = scratch.data;
  channel->scratch_capacity = scratch.capacity;
  channel->exchanges_left = options->exchanges;
  channel->max_command_bytes = options->max_command_bytes;
  channel->max_response_bytes = options->max_response_bytes;
  channel->flags = options->flags;
  channel->format = (uint8_t)options->format;
  channel->stopped = 0;
  return TC_APDU_OK;
}

/* A nonzero limit replaces the current one only when it is tighter. */
static size_t tighter(size_t current, size_t requested)
{
  return requested && (!current || requested < current) ? requested : current;
}

TC_APDU_result TC_APDU_channel_restrict(TC_APDU_channel* channel, size_t max_command_bytes,
                                        size_t max_response_bytes, unsigned flags)
{
  if (!channel || !channel->transport.transmit || (flags & ~(unsigned)KNOWN_FLAGS) ||
      !limits_valid(max_command_bytes, max_response_bytes))
    return TC_APDU_ARGUMENT;
  if (channel->stopped)
    return TC_APDU_ERROR;
  channel->max_command_bytes = tighter(channel->max_command_bytes, max_command_bytes);
  channel->max_response_bytes = tighter(channel->max_response_bytes, max_response_bytes);
  channel->flags = flags;
  return TC_APDU_OK;
}

/* Encode step into scratch, transmit it and check the transport contract.
 * The answer lands at response + state->used with the remaining capacity. */
static TC_APDU_result transmit_step(TC_APDU_channel* channel, const TC_APDU_command* step,
                                    exchange_state* state, size_t* received)
{
  tc_apdu_form form = {0, 0};
  TC_APDU_result result =
      tc_apdu_form_get(step->data.length, step->ne, (TC_APDU_length_format)state->format, &form);
  if (result != TC_APDU_OK)
    return result;
  const size_t offered = state->response.capacity - state->used;
  if (form.size > channel->scratch_capacity ||
      (channel->max_command_bytes && form.size > channel->max_command_bytes) ||
      offered < TC_APDU_STATUS_BYTES || !channel->exchanges_left)
    return TC_APDU_LIMIT;
  tc_apdu_write(step, &form, channel->scratch);
  --channel->exchanges_left;
  size_t length = 0;
  const TC_status status = channel->transport.transmit(
      channel->transport.context, (TC_bytes){channel->scratch, form.size},
      (TC_buffer){state->response.data + state->used, offered}, &length);
  /* Command fragments may carry PIN digits. */
  TC_secure_zero(channel->scratch, form.size);
  if (status != TC_OK || length < TC_APDU_STATUS_BYTES || length > offered) {
    channel->stopped = 1;
    return TC_APDU_ERROR;
  }
  if (state->used + length > state->high_water)
    state->high_water = state->used + length;
  *received = length;
  return TC_APDU_OK;
}

/* Read the answer at response + used, received bytes long. */
static TC_APDU_result answer_read(const exchange_state* state, size_t received,
                                  TC_APDU_response* answer)
{
  const TC_bytes bytes = {state->response.data + state->used, received};
  return TC_APDU_response_read(bytes, answer) == TC_APDU_OK ? TC_APDU_OK : TC_APDU_INVALID;
}

/* Send every fragment before the last (5.3.3). Returns OK with *done set when
 * an intermediate answer ended the chain, and advances *offset otherwise. */
static TC_APDU_result chain_send(TC_APDU_channel* channel, const TC_APDU_command* command,
                                 exchange_state* state, size_t* offset, int* done)
{
  TC_APDU_command fragment = *command;
  fragment.cla = (uint8_t)(command->cla | TC_APDU_CLA_CHAINING);
  fragment.ne = 0;
  while (command->data.length - *offset > TC_APDU_SHORT_MAX_NC) {
    fragment.data = (TC_bytes){command->data.data + *offset, TC_APDU_SHORT_MAX_NC};
    size_t received = 0;
    TC_APDU_response answer = {0};
    TC_APDU_result result = transmit_step(channel, &fragment, state, &received);
    if (result == TC_APDU_OK)
      result = answer_read(state, received, &answer);
    if (result != TC_APDU_OK)
      return result;
    /* 5.6: warnings are prohibited here, and 5.3.3 answers carry no data. */
    if (answer.data.length || TC_APDU_status_classify(answer.sw) == TC_APDU_SW_WARNING)
      return TC_APDU_INVALID;
    if (answer.sw != SUCCESS) {
      /* 6883, 6884 and other errors end the chain for the caller. */
      state->sw = answer.sw;
      *done = 1;
      return TC_APDU_OK;
    }
    *offset += TC_APDU_SHORT_MAX_NC;
  }
  return TC_APDU_OK;
}

/* Lower ne to the card's response buffer less SW1 SW2 (12.8.1). */
static uint32_t response_ne(const TC_APDU_channel* channel, uint32_t ne)
{
  if (channel->max_response_bytes && ne > channel->max_response_bytes - TC_APDU_STATUS_BYTES)
    return (uint32_t)(channel->max_response_bytes - TC_APDU_STATUS_BYTES);
  return ne;
}

/* GET RESPONSE after 61XX (5.3.4, 5.6). SW2 00 announces 256 or more bytes.
 * Each step stays within the card's response buffer. */
static TC_APDU_command get_response(const TC_APDU_channel* channel, uint8_t cla, uint8_t sw2)
{
  TC_APDU_command step = {{NULL, 0}, sw2, cla, GET_RESPONSE, 0, 0};
  if (channel->flags & TC_APDU_GET_RESPONSE_PLAIN_CLA)
    step.cla = (uint8_t)(cla & TC_APDU_CLA_CHANNEL_MASK);
  step.ne = response_ne(channel, sw2 ? sw2 : TC_APDU_SHORT_MAX_NE);
  return step;
}

/* Send the last or only command, then follow 61XX and 6CXX answers. */
static TC_APDU_result response_collect(TC_APDU_channel* channel, TC_APDU_command step, int chained,
                                       exchange_state* state)
{
  /* 6CXX correction is skipped under SM, where the logical command is
   * protected even when GET RESPONSE is plain (SP 800-73-5 Part 2 footnote
   * 25). Chained commands and commands without Le are never re-sent, so a
   * VERIFY with PIN digits reaches the card once. */
  const int sm = (step.cla & TC_APDU_CLA_SM_MASK) != 0;
  const uint8_t cla = step.cla;
  int correctable = !chained && !sm && step.ne, corrected = 0, card_stated = 0;
  for (;;) {
    /* A step whose Le the card stated needs room for Le and SW1 SW2. */
    if (card_stated && state->response.capacity - state->used < TC_APDU_RESPONSE_BYTES(step.ne))
      return TC_APDU_LIMIT;
    size_t received = 0;
    TC_APDU_response answer = {0};
    TC_APDU_result result = transmit_step(channel, &step, state, &received);
    if (result == TC_APDU_OK)
      result = answer_read(state, received, &answer);
    if (result != TC_APDU_OK)
      return result;
    state->sw = answer.sw;
    const uint8_t sw1 = (uint8_t)(answer.sw >> 8), sw2 = (uint8_t)answer.sw;
    if (sw1 == WRONG_LENGTH) {
      /* answer_read guarantees no data with 6CXX. */
      if (!correctable || corrected)
        return TC_APDU_OK;
      step.ne = sw2 ? sw2 : TC_APDU_SHORT_MAX_NE;
      corrected = 1;
      card_stated = 1;
      continue;
    }
    /* The next chunk overwrites these status bytes. */
    state->used += answer.data.length;
    if (sw1 != MORE_DATA)
      return TC_APDU_OK;
    step = get_response(channel, cla, sw2);
    correctable = !sm;
    corrected = 0;
    card_stated = 1;
  }
}

/* Check a SHORT chain of nc data bytes before its first transmit, so the
 * scratch, the card limit or the budget cannot interrupt it (5.3.3). */
static TC_APDU_result chain_check(const TC_APDU_channel* channel, size_t nc, uint32_t ne)
{
  const size_t fragments = nc / TC_APDU_SHORT_MAX_NC + (nc % TC_APDU_SHORT_MAX_NC != 0);
  const size_t last = nc - (fragments - 1) * TC_APDU_SHORT_MAX_NC;
  /* Intermediate fragments carry 255 bytes and no Le. The last adds Le. */
  size_t largest = TC_APDU_HEADER_BYTES + 1 + TC_APDU_SHORT_MAX_NC;
  const size_t final_size = TC_APDU_HEADER_BYTES + 1 + last + (ne ? 1 : 0);
  if (final_size > largest)
    largest = final_size;
  if (largest > channel->scratch_capacity ||
      (channel->max_command_bytes && largest > channel->max_command_bytes) ||
      channel->exchanges_left < fragments)
    return TC_APDU_LIMIT;
  return TC_APDU_OK;
}

TC_APDU_result tc_apdu_channel_fits(const TC_APDU_channel* channel, TC_APDU_length_format format,
                                    size_t nc, uint32_t ne)
{
  if (format == TC_APDU_SHORT && nc > TC_APDU_SHORT_MAX_NC && nc <= TC_APDU_MAX_NC)
    return chain_check(channel, nc, ne);
  tc_apdu_form form = {0, 0};
  const TC_APDU_result result = tc_apdu_form_get(nc, ne, format, &form);
  if (result != TC_APDU_OK)
    return result;
  if (form.size > channel->scratch_capacity ||
      (channel->max_command_bytes && form.size > channel->max_command_bytes) ||
      !channel->exchanges_left)
    return TC_APDU_LIMIT;
  return TC_APDU_OK;
}

static TC_APDU_result transceive_run(TC_APDU_channel* channel, const TC_APDU_command* command,
                                     exchange_state* state)
{
  TC_APDU_command step = *command;
  size_t offset = 0;
  int chained = 0, done = 0;
  if (state->format == TC_APDU_SHORT && command->data.length > TC_APDU_SHORT_MAX_NC) {
    chained = 1;
    TC_APDU_result result = chain_check(channel, command->data.length, command->ne);
    if (result == TC_APDU_OK)
      result = chain_send(channel, command, state, &offset, &done);
    if (result != TC_APDU_OK || done)
      return result;
    step.data = (TC_bytes){command->data.data + offset, command->data.length - offset};
  }
  step.ne = response_ne(channel, step.ne);
  return response_collect(channel, step, chained, state);
}

/* Argument checks at the public entry. */
static TC_APDU_result transceive_check(const TC_APDU_channel* channel, TC_APDU_length_format format,
                                       const TC_APDU_command* command, TC_buffer response,
                                       const TC_APDU_response* out)
{
  if (!channel || !command || !out || !channel->transport.transmit || !response.data ||
      response.capacity < TC_APDU_STATUS_BYTES ||
      !tc_internal_span_valid(command->data.data, command->data.length))
    return TC_APDU_ARGUMENT;
  tc_apdu_form form = {0, 0};
  /* SHORT sends data above 255 bytes as a chain, so only its Le bound
   * applies to the whole command. */
  size_t nc = command->data.length;
  if (format == TC_APDU_SHORT && nc > TC_APDU_SHORT_MAX_NC && nc <= TC_APDU_MAX_NC)
    nc = TC_APDU_SHORT_MAX_NC;
  TC_APDU_result result = tc_apdu_form_get(nc, command->ne, format, &form);
  if (result == TC_APDU_OK)
    result = tc_apdu_class_check(command->cla);
  if (result != TC_APDU_OK)
    return result;
  const TC_bytes data = command->data;
  const uint8_t* scratch = channel->scratch;
  const size_t scratch_capacity = channel->scratch_capacity;
  if (!tc_internal_ranges_disjoint(response.data, response.capacity, scratch, scratch_capacity) ||
      !tc_internal_ranges_disjoint(response.data, response.capacity, data.data, data.length) ||
      !tc_internal_ranges_disjoint(response.data, response.capacity, channel, sizeof *channel) ||
      !tc_internal_ranges_disjoint(response.data, response.capacity, command, sizeof *command) ||
      !tc_internal_ranges_disjoint(response.data, response.capacity, out, sizeof *out) ||
      !tc_internal_ranges_disjoint(scratch, scratch_capacity, data.data, data.length) ||
      !tc_internal_ranges_disjoint(scratch, scratch_capacity, command, sizeof *command) ||
      !tc_internal_ranges_disjoint(scratch, scratch_capacity, out, sizeof *out))
    return TC_APDU_ARGUMENT;
  return TC_APDU_OK;
}

TC_APDU_result tc_apdu_transceive_format(TC_APDU_channel* channel, TC_APDU_length_format format,
                                         const TC_APDU_command* command, TC_buffer response,
                                         TC_APDU_response* out)
{
  const TC_APDU_result checked = transceive_check(channel, format, command, response, out);
  if (checked != TC_APDU_OK)
    return checked;
  if (channel->stopped)
    return TC_APDU_ERROR;
  exchange_state state = {response, 0, 0, 0, (uint8_t)format};
  const TC_APDU_result result = transceive_run(channel, command, &state);
  if (result != TC_APDU_OK) {
    /* The buffer may hold a partial, unauthenticated or secret answer. */
    TC_secure_zero(response.data, response.capacity);
    return result;
  }
  /* Wipe the last status bytes and any 6CXX answers after the data. */
  if (state.high_water > state.used)
    TC_secure_zero(response.data + state.used, state.high_water - state.used);
  out->data.data = response.data;
  out->data.length = state.used;
  out->sw = state.sw;
  return TC_APDU_OK;
}

TC_APDU_result TC_APDU_transceive(TC_APDU_channel* channel, const TC_APDU_command* command,
                                  TC_buffer response, TC_APDU_response* out)
{
  if (!channel)
    return TC_APDU_ARGUMENT;
  return tc_apdu_transceive_format(channel, (TC_APDU_length_format)channel->format, command,
                                   response, out);
}

size_t TC_APDU_channel_exchanges_left(const TC_APDU_channel* channel)
{
  return channel ? channel->exchanges_left : 0;
}

void TC_APDU_channel_clear(TC_APDU_channel* channel)
{
  if (!channel)
    return;
  if (channel->scratch)
    TC_secure_zero(channel->scratch, channel->scratch_capacity);
  TC_secure_zero(channel, sizeof *channel);
}
#endif
