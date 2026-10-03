/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* VERIFY: retry query and PIN submission (SP 800-73-5 Part 2 2.4.3,
 * 3.2.1 and 3.2.1.1). */
#include <tiny_crypto/piv_command.h>
#if TC_ENABLE_PIV_COMMAND
#include "internal.h"
#include "piv_link_internal.h"

enum {
  VERIFY = 0x20,
  REFERENCE_GLOBAL_PIN = 0x00,
  REFERENCE_PIN = 0x80,
  REFERENCE_OCC_FIRST = 0x96,
  REFERENCE_OCC_SECOND = 0x97,
  REFERENCE_PAIRING_CODE = 0x98,
  /* Part 2 2.4.3: 6 to 8 ASCII digits padded with FF to 8 bytes. */
  PIN_MIN_DIGITS = 6,
  PIN_BYTES = 8,
  PIN_PADDING = 0xff,
  /* 63CX carries a 4-bit counter, and a floor below 2 would spend the last
   * try. */
  RETRIES_FLOOR = 2,
  RETRIES_MAXIMUM = 15,
  RETRY_STATUS = 0x63c0,
  NO_COUNT_STATUS = 0x6300,
  /* DO 99 (4), DO 8E (10) and SW1 SW2. */
  VERIFY_ANSWER_BYTES = 4 + 10 + TC_APDU_STATUS_BYTES
};

static int pin_reference(uint8_t reference)
{
  return reference == REFERENCE_PIN || reference == REFERENCE_GLOBAL_PIN;
}

/* Refuse what the card must reject (Part 2 3.2.1): PIN references outside
 * the contact interface and the VCI, and the pairing code on contactless
 * without secure messaging. The PIN never crosses RF in plaintext (Part 1
 * Table 4). */
static TC_PIV_result verify_allowed(const TC_PIV_link* link, uint8_t reference)
{
  if (link->application == TC_PIV_APPLICATION_NONE)
    return TC_PIV_REFUSED;
  if (link->application != TC_PIV_APPLICATION_PIV)
    return TC_PIV_UNSUPPORTED;
  if (link->interface != TC_PIV_CONTACTLESS)
    return TC_PIV_OK;
  /* The VCI exists only on a secured link (Part 1 Table 2 footnote 9). */
  if (pin_reference(reference))
    return (link->flags & TC_PIV_LINK_VCI) && (link->flags & TC_PIV_LINK_SECURED) ? TC_PIV_OK
                                                                                  : TC_PIV_REFUSED;
  return link->flags & TC_PIV_LINK_SECURED ? TC_PIV_OK : TC_PIV_REFUSED;
}

/* Send one VERIFY with P1 00 and no Le. The answer carries SW1 SW2, and
 * under secure messaging 99 02 SW 8E 08 MAC before them (Part 2 4.2.6). */
static TC_PIV_result verify_send(TC_PIV_link* link, uint8_t reference, TC_bytes data, uint16_t* sw)
{
  uint8_t answer_bytes[VERIFY_ANSWER_BYTES];
  const TC_APDU_command command = {data, 0, TC_PIV_PLAIN_CLA, VERIFY, 0x00, reference};
  TC_APDU_response answer;
  const TC_buffer buffer = {answer_bytes, sizeof answer_bytes};
  const TC_PIV_result result =
      tc_piv_link_transceive(link, TC_PIV_COMMAND_VERIFY, &command, buffer, &answer);
  if (result != TC_PIV_OK)
    return result;
  /* VERIFY answers carry no data (Part 2 3.2.1). */
  if (answer.data.length)
    return tc_piv_link_fail(link, buffer, 0, TC_PIV_INVALID);
  *sw = answer.sw;
  return TC_PIV_OK;
}

/* Record the card's answer for a PIN reference: only 9000 means its
 * security status is TRUE (Part 2 3.2.1.1). The link keeps one PIN flag for
 * 80 and 00, so a negative answer for either clears it. */
static void pin_status_record(TC_PIV_link* link, uint8_t reference, uint16_t sw)
{
  if (!pin_reference(reference))
    return;
  if (sw == TC_PIV_SW_SUCCESS_VALUE)
    link->flags |= TC_PIV_LINK_PIN_VERIFIED;
  else
    link->flags &= (uint8_t)~TC_PIV_LINK_PIN_VERIFIED;
}

/* Interpret a retry query: 9000 verified, 63CX with a count, 6300 without
 * one (Part 2 footnote 7). Other statuses are CARD_STATUS. */
static TC_PIV_result query_result(TC_PIV_link* link, uint8_t reference, uint16_t sw,
                                  TC_PIV_reference_status* status)
{
  status->retries = 0;
  status->verified = 0;
  status->submitted = 0;
  status->retries_known = 0;
  pin_status_record(link, reference, sw);
  if (sw == TC_PIV_SW_SUCCESS_VALUE) {
    status->verified = 1;
    return TC_PIV_OK;
  }
  if ((sw & 0xfff0) == RETRY_STATUS) {
    status->retries = sw & 0x0f;
    status->retries_known = 1;
    return TC_PIV_OK;
  }
  return sw == NO_COUNT_STATUS ? TC_PIV_OK : TC_PIV_CARD_STATUS;
}

TC_PIV_result TC_PIV_verify_status(TC_PIV_link* link, uint8_t reference,
                                   TC_PIV_reference_status* out)
{
  if (!tc_piv_link_ready(link) || !out)
    return TC_PIV_ARGUMENT;
  if (reference == REFERENCE_OCC_FIRST || reference == REFERENCE_OCC_SECOND)
    return TC_PIV_UNSUPPORTED;
  if (!pin_reference(reference) && reference != REFERENCE_PAIRING_CODE)
    return TC_PIV_ARGUMENT;
  TC_PIV_result result = verify_allowed(link, reference);
  if (result != TC_PIV_OK)
    return result;
  uint16_t sw = 0;
  result = verify_send(link, reference, (TC_bytes){NULL, 0}, &sw);
  if (result != TC_PIV_OK)
    return result;
  TC_PIV_reference_status status;
  result = query_result(link, reference, sw, &status);
  if (result == TC_PIV_OK)
    *out = status;
  return result;
}

int tc_piv_digits_valid(TC_bytes digits, size_t minimum)
{
  if (!digits.data || digits.length < minimum || digits.length > PIN_BYTES)
    return 0;
  for (size_t i = 0; i < digits.length; ++i)
    if (digits.data[i] < '0' || digits.data[i] > '9')
      return 0;
  return 1;
}

TC_PIV_result tc_piv_verify_submit(TC_PIV_link* link, uint8_t reference, TC_bytes digits,
                                   uint16_t* sw)
{
  uint8_t padded[PIN_BYTES];
  memset(padded, PIN_PADDING, sizeof padded);
  memcpy(padded, digits.data, digits.length);
  const TC_PIV_result result = verify_send(link, reference, (TC_bytes){padded, sizeof padded}, sw);
  TC_secure_zero(padded, sizeof padded);
  return result;
}

/* Submit the padded PIN once. No retry and no length correction. */
static TC_PIV_result pin_submit(TC_PIV_link* link, uint8_t reference, TC_bytes pin,
                                TC_PIV_reference_status* status)
{
  uint16_t sw = 0;
  const TC_PIV_result result = tc_piv_verify_submit(link, reference, pin, &sw);
  if (result != TC_PIV_OK)
    return result;
  pin_status_record(link, reference, sw);
  if (sw != TC_PIV_SW_SUCCESS_VALUE)
    return TC_PIV_CARD_STATUS;
  /* Success resets the counter to an unreported value. */
  status->retries = 0;
  status->retries_known = 0;
  status->verified = 1;
  status->submitted = 1;
  return TC_PIV_OK;
}

TC_PIV_result TC_PIV_pin_verify(TC_PIV_link* link, uint8_t reference, TC_bytes pin,
                                unsigned minimum_retries, TC_PIV_reference_status* out)
{
  if (!tc_piv_link_ready(link) || !out || !pin_reference(reference) ||
      !tc_piv_digits_valid(pin, PIN_MIN_DIGITS) || minimum_retries < RETRIES_FLOOR ||
      minimum_retries > RETRIES_MAXIMUM || !tc_piv_link_disjoint(link, pin.data, pin.length) ||
      !tc_internal_ranges_disjoint(pin.data, pin.length, out, sizeof *out))
    return TC_PIV_ARGUMENT;
  TC_PIV_result result = verify_allowed(link, reference);
  if (result != TC_PIV_OK)
    return result;
  uint16_t sw = 0;
  result = verify_send(link, reference, (TC_bytes){NULL, 0}, &sw);
  if (result != TC_PIV_OK)
    return result;
  TC_PIV_reference_status status;
  result = query_result(link, reference, sw, &status);
  if (result != TC_PIV_OK)
    return result;
  if (!status.verified) {
    /* Keep the last tries unspent. A card without a count gives no floor. */
    if (!status.retries_known || status.retries < minimum_retries)
      return TC_PIV_REFUSED;
    result = pin_submit(link, reference, pin, &status);
    if (result != TC_PIV_OK)
      return result;
  }
  *out = status;
  return TC_PIV_OK;
}
#endif
