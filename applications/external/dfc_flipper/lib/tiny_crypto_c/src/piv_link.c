/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Link lifecycle, state and the command dispatch of the PIV card commands. */
#include <tiny_crypto/piv_command.h>
#if TC_ENABLE_PIV_COMMAND
#include "internal.h"
#include "piv_link_internal.h"
#if TC_ENABLE_PIV_SM_APDU
#include <tiny_crypto/piv_sm.h>
#endif

/* Smallest command scratch for format: every SHORT fragment, or the largest
 * EXTENDED command of this module within the card limit. */
static size_t scratch_minimum(const TC_APDU_channel_options* options)
{
  if (options->format == TC_APDU_SHORT)
    return TC_APDU_SHORT_COMMAND_MAX_BYTES;
  const size_t largest = TC_PIV_EXTENDED_SCRATCH_BYTES;
  return options->max_command_bytes && options->max_command_bytes < largest
             ? options->max_command_bytes
             : largest;
}

/* 0 selects Le 00. SHORT allows up to 256 and EXTENDED up to 65536
 * (ISO/IEC 7816-4 5.2). */
static int response_ne_valid(const TC_PIV_link_options* options)
{
  const uint32_t maximum =
      options->channel.format == TC_APDU_EXTENDED ? TC_APDU_MAX_NE : TC_APDU_SHORT_MAX_NE;
  return options->response_ne <= maximum;
}

TC_PIV_result TC_PIV_link_init(TC_PIV_link* link, TC_APDU_transport transport,
                               const TC_PIV_link_options* options, TC_buffer command_scratch)
{
  if (!link || !options || !command_scratch.data ||
      (options->interface != TC_PIV_CONTACT && options->interface != TC_PIV_CONTACTLESS) ||
      (options->channel.format != TC_APDU_SHORT && options->channel.format != TC_APDU_EXTENDED) ||
      !response_ne_valid(options) ||
      command_scratch.capacity < scratch_minimum(&options->channel) ||
      !tc_internal_ranges_disjoint(command_scratch.data, command_scratch.capacity, link,
                                   sizeof *link) ||
      !tc_internal_ranges_disjoint(command_scratch.data, command_scratch.capacity, options,
                                   sizeof *options))
    return TC_PIV_ARGUMENT;
  /* The channel checks the remaining options and writes only on success. */
  if (TC_APDU_channel_init(&link->channel, transport, &options->channel, command_scratch) !=
      TC_APDU_OK)
    return TC_PIV_ARGUMENT;
  link->security = NULL;
  link->sm = NULL;
  link->sm_workspace = NULL;
  link->sm_scratch = NULL;
  link->sm_scratch_capacity = 0;
  link->response_ne = options->response_ne ? options->response_ne : TC_APDU_SHORT_MAX_NE;
  link->status = 0;
  link->interface = (uint8_t)options->interface;
  link->application = TC_PIV_APPLICATION_NONE;
  link->profile = TC_PIV_CARD;
  link->command = TC_PIV_COMMAND_SELECT;
  link->sm_suite = 0;
  link->flags = 0;
  return TC_PIV_OK;
}

void TC_PIV_link_info_get(const TC_PIV_link* link, TC_PIV_link_info* out)
{
  if (!link || !out)
    return;
  out->interface = (TC_PIV_interface)link->interface;
  out->application = (TC_PIV_application_id)link->application;
  out->profile = (TC_PIV_card_profile)link->profile;
  out->secured = (link->flags & TC_PIV_LINK_SECURED) != 0;
  out->sm_lost = (link->flags & TC_PIV_LINK_SM_LOST) != 0;
  out->vci = (link->flags & TC_PIV_LINK_VCI) != 0;
  out->pin_verified = (link->flags & TC_PIV_LINK_PIN_VERIFIED) != 0;
  out->sm_suite = link->sm_suite;
}

uint16_t TC_PIV_link_status(const TC_PIV_link* link)
{
  return link ? link->status : 0;
}

void tc_piv_link_unbind(TC_PIV_link* link)
{
  if (link->security)
    link->security->unbind(link);
  link->flags &= (uint8_t)~TC_PIV_LINK_SECURED;
}

void tc_piv_link_session_lost(TC_PIV_link* link)
{
  tc_piv_link_unbind(link);
  link->flags |= TC_PIV_LINK_SM_LOST;
  link->flags &= (uint8_t)~(TC_PIV_LINK_VCI | TC_PIV_LINK_PIN_VERIFIED);
}

void TC_PIV_link_clear(TC_PIV_link* link)
{
  if (!link)
    return;
  tc_piv_link_unbind(link);
  TC_APDU_channel_clear(&link->channel);
  TC_secure_zero(link, sizeof *link);
}

int tc_piv_link_ready(const TC_PIV_link* link)
{
  return link && link->channel.transport.transmit;
}

int tc_piv_link_disjoint(const TC_PIV_link* link, const void* data, size_t length)
{
  if (!tc_internal_ranges_disjoint(data, length, link, sizeof *link) ||
      !tc_internal_ranges_disjoint(data, length, link->channel.scratch,
                                   link->channel.scratch_capacity))
    return 0;
#if TC_ENABLE_PIV_SM_APDU
  /* A bound session and its storage change during every protected command. */
  if (link->sm &&
      (!tc_internal_ranges_disjoint(data, length, link->sm, sizeof(TC_PIV_SM)) ||
       !tc_internal_ranges_disjoint(data, length, link->sm_workspace,
                                    link->sm_workspace ? sizeof(TC_PIV_SM_workspace) : 0) ||
       !tc_internal_ranges_disjoint(data, length, link->sm_scratch, link->sm_scratch_capacity)))
    return 0;
#endif
  return 1;
}

int tc_piv_response_valid(const TC_PIV_link* link, TC_buffer response, const void* out,
                          size_t out_length)
{
  return response.data && response.capacity >= TC_APDU_STATUS_BYTES &&
         tc_piv_link_disjoint(link, response.data, response.capacity) &&
         tc_internal_ranges_disjoint(response.data, response.capacity, out, out_length);
}

TC_PIV_result tc_piv_channel_result(TC_APDU_result result)
{
  switch (result) {
  case TC_APDU_OK:
    return TC_PIV_OK;
  case TC_APDU_INVALID:
    return TC_PIV_INVALID;
  case TC_APDU_LIMIT:
    return TC_PIV_LIMIT;
  case TC_APDU_ARGUMENT:
    return TC_PIV_ARGUMENT;
  case TC_APDU_UNSUPPORTED:
    return TC_PIV_UNSUPPORTED;
  default:
    return TC_PIV_ERROR;
  }
}

static int sm_eligible(uint8_t ins)
{
  return ins == TC_PIV_INS_GET_DATA || ins == TC_PIV_INS_VERIFY ||
         ins == TC_PIV_INS_GENERAL_AUTHENTICATE;
}

TC_PIV_result tc_piv_link_transceive(TC_PIV_link* link, TC_PIV_command kind,
                                     const TC_APDU_command* command, TC_buffer response,
                                     TC_APDU_response* out)
{
  /* A secured link, or one whose session was lost, never sends these
   * commands in plaintext (SP 800-73-5 Part 2 4.2 and 4.3). */
  const int protect =
      sm_eligible(command->ins) && (link->flags & (TC_PIV_LINK_SECURED | TC_PIV_LINK_SM_LOST)) != 0;
  /* Nothing is sent, so the last card status stays. */
  if (protect && (!(link->flags & TC_PIV_LINK_SECURED) || !link->security)) {
    TC_secure_zero(response.data, response.capacity);
    return TC_PIV_REFUSED;
  }
  link->command = (uint8_t)kind;
  link->status = 0;
  const TC_PIV_result result =
      protect ? link->security->transceive(link, command, response, out)
              : tc_piv_channel_result(TC_APDU_transceive(&link->channel, command, response, out));
  if (result == TC_PIV_OK) {
    link->status = out->sw;
    return result;
  }
  /* The stopped channel cannot carry the session further, so its keys go now
   * (Part 2 4.3). */
  if (result == TC_PIV_ERROR && link->security)
    tc_piv_link_session_lost(link);
  /* Every failed exchange leaves no partial answer in the response. */
  TC_secure_zero(response.data, response.capacity);
  return result;
}

TC_PIV_result tc_piv_link_fail(TC_PIV_link* link, TC_buffer response, uint16_t status,
                               TC_PIV_result result)
{
  TC_secure_zero(response.data, response.capacity);
  link->status = status;
  return result;
}
#endif
