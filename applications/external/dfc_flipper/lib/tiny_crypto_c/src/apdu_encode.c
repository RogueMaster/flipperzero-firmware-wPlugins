/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Command APDU encoding (ISO/IEC 7816-4:2020 5.2). */
#include <tiny_crypto/apdu.h>
#if TC_ENABLE_APDU
#include "apdu_internal.h"
#include "internal.h"
#include <string.h>

TC_APDU_result tc_apdu_class_check(uint8_t cla)
{
  /* 5.4.1: b8 marks the proprietary class, 001x xxxx is RFU and 01xx xxxx
   * holds the further interindustry values of Table 3. */
  if (cla & TC_APDU_CLA_CLASS_MASK)
    return TC_APDU_UNSUPPORTED;
  /* 5.3.3: b5 is the chaining indication, set only by the channel. */
  return cla & TC_APDU_CLA_CHAINING ? TC_APDU_ARGUMENT : TC_APDU_OK;
}

TC_APDU_result tc_apdu_form_get(size_t nc, uint32_t ne, TC_APDU_length_format format,
                                tc_apdu_form* out)
{
  if ((format != TC_APDU_SHORT && format != TC_APDU_EXTENDED) || nc > TC_APDU_MAX_NC ||
      ne > TC_APDU_MAX_NE)
    return TC_APDU_ARGUMENT;
  /* 5.2: short and extended fields are never mixed in one command. EXTENDED
   * keeps the short form when both fields fit it. */
  const int short_fits = nc <= TC_APDU_SHORT_MAX_NC && ne <= TC_APDU_SHORT_MAX_NE;
  if (short_fits) {
    out->size = TC_APDU_HEADER_BYTES + (nc ? 1 + nc : 0) + (ne ? 1 : 0);
    out->extended = 0;
    return TC_APDU_OK;
  }
  if (format == TC_APDU_SHORT)
    return TC_APDU_ARGUMENT;
  /* Extended Lc is 00 HI LO. Extended Le is HI LO after an extended Lc, and
   * 00 HI LO without data. The sum stays within 9 + nc. */
  if (nc > SIZE_MAX - 9)
    return TC_APDU_LIMIT;
  out->size = TC_APDU_HEADER_BYTES + (nc ? 3 + nc : 0) + (ne ? (nc ? 2 : 3) : 0);
  out->extended = 1;
  return TC_APDU_OK;
}

void tc_apdu_write(const TC_APDU_command* command, const tc_apdu_form* form, uint8_t* out)
{
  const size_t nc = command->data.length;
  const uint32_t ne = command->ne;
  size_t used = 0;
  out[used++] = command->cla;
  out[used++] = command->ins;
  out[used++] = command->p1;
  out[used++] = command->p2;
  if (nc) {
    if (form->extended) {
      out[used++] = 0;
      out[used++] = (uint8_t)(nc >> 8);
    }
    out[used++] = (uint8_t)nc;
    memcpy(out + used, command->data.data, nc);
    used += nc;
  }
  if (ne) {
    /* 5.2: Ne 256 encodes short 00 and Ne 65536 encodes extended 0000. The
     * casts keep the low bytes, which gives both zero encodings. */
    if (form->extended) {
      if (!nc)
        out[used++] = 0;
      out[used++] = (uint8_t)(ne >> 8);
    }
    out[used] = (uint8_t)ne;
  }
}

/* Argument checks shared by size and encode. */
static TC_APDU_result command_check(const TC_APDU_command* command, TC_APDU_length_format format,
                                    tc_apdu_form* form)
{
  if (!command || !tc_internal_span_valid(command->data.data, command->data.length))
    return TC_APDU_ARGUMENT;
  TC_APDU_result result = tc_apdu_form_get(command->data.length, command->ne, format, form);
  if (result == TC_APDU_OK)
    result = tc_apdu_class_check(command->cla);
  return result;
}

TC_APDU_result TC_APDU_command_size(const TC_APDU_command* command, TC_APDU_length_format format,
                                    size_t* size)
{
  tc_apdu_form form = {0, 0};
  if (!size)
    return TC_APDU_ARGUMENT;
  const TC_APDU_result result = command_check(command, format, &form);
  if (result == TC_APDU_OK)
    *size = form.size;
  return result;
}

TC_APDU_result TC_APDU_command_encode(const TC_APDU_command* command, TC_APDU_length_format format,
                                      TC_buffer out, size_t* written)
{
  tc_apdu_form form = {0, 0};
  if (!written || !tc_internal_span_valid(out.data, out.capacity))
    return TC_APDU_ARGUMENT;
  TC_APDU_result result = command_check(command, format, &form);
  if (result != TC_APDU_OK)
    return result;
  if (!tc_internal_ranges_disjoint(out.data, out.capacity, command->data.data,
                                   command->data.length) ||
      !tc_internal_ranges_disjoint(out.data, out.capacity, command, sizeof *command) ||
      !tc_internal_ranges_disjoint(out.data, out.capacity, written, sizeof *written))
    return TC_APDU_ARGUMENT;
  if (form.size > out.capacity)
    return TC_APDU_LIMIT;
  tc_apdu_write(command, &form, out.data);
  *written = form.size;
  return TC_APDU_OK;
}
#endif
