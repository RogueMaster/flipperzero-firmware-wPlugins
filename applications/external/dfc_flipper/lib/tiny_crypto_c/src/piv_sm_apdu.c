/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV secure messaging wire format: protected commands (SP 800-73-5 Part 2
 * 4.2.2 to 4.2.4), protected responses (4.2.5 to 4.2.7), the session-loss
 * rule (4.3, footnote 25) and link securing. */
#include <tiny_crypto/piv_sm_apdu.h>
#if TC_ENABLE_PIV_SM_APDU
#include "apdu_internal.h"
#include "internal.h"
#include "piv_link_internal.h"
#include "piv_sm_apdu_internal.h"
#include "tlv_internal.h"

enum {
  SM_CLA = 0x0c, /* CLA with b4 b3 = 11: SM with the header authenticated */
  CRYPTOGRAM = 0x87,
  PADDING_INDICATOR = 0x01,
  LE_OBJECT = 0x97,
  STATUS_OBJECT = 0x99,
  MAC_OBJECT = 0x8e,
  MAC_BYTES = 8,
  /* 97 01 00 and 8E 08 MAC. */
  LE_OBJECT_BYTES = 3,
  MAC_OBJECT_BYTES = 2 + MAC_BYTES,
  BLOCK = 16
};

static const uint8_t cryptogram_tag = CRYPTOGRAM;

/* Unbind after a failure: the session is gone on both sides, so the link
 * refuses protected commands until TC_PIV_link_unsecure. */
static TC_PIV_result session_lost(TC_PIV_link* link, TC_buffer response, uint16_t status,
                                  TC_PIV_result result)
{
  tc_piv_link_session_lost(link);
  return tc_piv_link_fail(link, response, status, result);
}

static void sm_unbind(TC_PIV_link* link)
{
  TC_PIV_SM_clear((TC_PIV_SM*)link->sm);
  if (link->sm_scratch)
    TC_secure_zero(link->sm_scratch, link->sm_scratch_capacity);
  link->security = NULL;
  link->sm = NULL;
  link->sm_workspace = NULL;
  link->sm_scratch = NULL;
  link->sm_scratch_capacity = 0;
  link->flags &= (uint8_t)~TC_PIV_LINK_SECURED;
}

/* Layout of one protected command in the SM scratch:
 * [87 L 01 ciphertext] [97 01 00] [8E 08 MAC]. */
typedef struct {
  size_t ciphertext_offset, ciphertext_length;
  size_t authenticated_length; /* 87 and 97, the bytes after the header block */
  size_t field_length;         /* the complete SM data field */
} command_layout;

/* Size the field for nc plain bytes. 0 when nc exceeds the 3-octet 87
 * length the Nc bound allows. */
static int layout_get(size_t nc, int has_le, command_layout* layout)
{
  size_t header = 0;
  layout->ciphertext_length = 0;
  if (nc) {
    if (nc > TC_PIV_SM_MAX_PLAIN_NC)
      return 0;
    /* ISO 7816-4 padding always adds 1 to 16 bytes (4.2.2). */
    layout->ciphertext_length = TC_PIV_PADDED_BYTES(nc);
    header = tc_tlv_header_size(1, layout->ciphertext_length + 1) + 1;
  }
  layout->ciphertext_offset = header;
  layout->authenticated_length =
      header + layout->ciphertext_length + (has_le ? LE_OBJECT_BYTES : 0);
  layout->field_length = layout->authenticated_length + MAC_OBJECT_BYTES;
  return 1;
}

/* Build the SM data field for command in the SM scratch (4.2.3, 4.2.4). The
 * C-MAC covers the header block 0C INS P1 P2 80 00..00 and the 87 and 97 DOs.
 * Footnote 20: the 97 value is 00 whenever Le is present. */
static TC_status command_protect(TC_PIV_link* link, const TC_APDU_command* command,
                                 const command_layout* layout)
{
  uint8_t* field = link->sm_scratch;
  uint8_t header[BLOCK] = {SM_CLA, command->ins, command->p1, command->p2, 0x80};
  if (layout->ciphertext_length) {
    uint8_t* end = tc_tlv_header_write(field, &cryptogram_tag, 1, layout->ciphertext_length + 1);
    *end = PADDING_INDICATOR;
  }
  if (command->ne) {
    uint8_t* le = field + layout->ciphertext_offset + layout->ciphertext_length;
    le[0] = LE_OBJECT;
    le[1] = 1;
    le[2] = 0;
  }
  const TC_bytes authenticated[] = {{header, sizeof header}, {field, layout->authenticated_length}};
  const TC_PIV_SM_protect_request request = {
      command->data,
      {layout->ciphertext_length ? field + layout->ciphertext_offset : NULL,
       layout->ciphertext_length},
      authenticated,
      2};
  uint8_t* mac = field + layout->authenticated_length;
  size_t written = 0;
  mac[0] = MAC_OBJECT;
  mac[1] = MAC_BYTES;
  return TC_PIV_SM_protect((TC_PIV_SM*)link->sm, &request, &written,
                           (TC_buffer){mac + 2, MAC_BYTES},
                           (TC_PIV_SM_workspace*)link->sm_workspace);
}

static int tag_is(const TC_TLV_element* element, uint8_t tag)
{
  return element->header.tag_length == 1 && element->header.tag[0] == tag;
}

/* The DOs of a protected answer, borrowed from its data. */
typedef struct {
  TC_bytes ciphertext, authenticated, mac;
  uint16_t sw;
} response_fields;

/* Read [87 L 01 ciphertext] 99 02 SW1 SW2 8E 08 MAC with nothing after it
 * (4.2.5, 4.2.6). 87 needs at least one block, a multiple of 16 bytes and
 * indicator 01. A VERIFY answer carries no 87. */
static int response_read(TC_bytes data, uint8_t ins, response_fields* out)
{
  const TC_TLV_limits limits = {data.length, data.length, 3, 0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  out->ciphertext = (TC_bytes){NULL, 0};
  if (TC_TLV_reader_init(&reader, data, TC_TLV_ISO7816, &limits) != TC_TLV_OK ||
      TC_TLV_next(&reader, &element) != TC_TLV_OK)
    return 0;
  if (tag_is(&element, CRYPTOGRAM)) {
    const TC_bytes value = element.value;
    if (ins == TC_PIV_INS_VERIFY || value.length < 1 + BLOCK ||
        value.data[0] != PADDING_INDICATOR || (value.length - 1) % BLOCK)
      return 0;
    out->ciphertext = (TC_bytes){value.data + 1, value.length - 1};
    if (TC_TLV_next(&reader, &element) != TC_TLV_OK)
      return 0;
  }
  if (!tag_is(&element, STATUS_OBJECT) || element.value.length != 2)
    return 0;
  out->sw = (uint16_t)((uint16_t)element.value.data[0] << 8 | element.value.data[1]);
  out->authenticated = (TC_bytes){data.data, reader.offset};
  if (TC_TLV_next(&reader, &element) != TC_TLV_OK || !tag_is(&element, MAC_OBJECT) ||
      element.value.length != MAC_BYTES || reader.offset != data.length)
    return 0;
  out->mac = element.value;
  return 1;
}

/* Check the R-MAC, then decrypt in place: the plaintext replaces the
 * ciphertext inside response. The inner status counts only after
 * TC_PIV_SM_unprotect returned TC_OK. */
static TC_status response_unprotect(TC_PIV_link* link, TC_buffer response,
                                    const response_fields* fields, uint8_t** plaintext,
                                    size_t* plain_length)
{
  const TC_PIV_SM_unprotect_request request = {fields->ciphertext, fields->mac,
                                               &fields->authenticated, 1};
  *plaintext = fields->ciphertext.length
                   ? response.data + (size_t)(fields->ciphertext.data - response.data)
                   : response.data;
  const TC_buffer output = {fields->ciphertext.length ? *plaintext : NULL,
                            fields->ciphertext.length};
  return TC_PIV_SM_unprotect((TC_PIV_SM*)link->sm, &request, output, plain_length,
                             (TC_PIV_SM_workspace*)link->sm_workspace);
}

/* LIMIT before protection keeps the session: the command must fit the SM
 * scratch, the channel scratch, the card limit and the exchange budget as
 * SHORT fragments. */
static TC_PIV_result command_fits(const TC_PIV_link* link, const TC_APDU_command* command,
                                  command_layout* layout)
{
  if (command->cla != TC_PIV_PLAIN_CLA ||
      !layout_get(command->data.length, command->ne != 0, layout) ||
      !tc_internal_ranges_disjoint(command->data.data, command->data.length, link->sm_scratch,
                                   link->sm_scratch_capacity))
    return TC_PIV_ARGUMENT;
  if (layout->field_length > link->sm_scratch_capacity)
    return TC_PIV_LIMIT;
  return tc_piv_channel_result(tc_apdu_channel_fits(&link->channel, TC_APDU_SHORT,
                                                    layout->field_length, TC_APDU_SHORT_MAX_NE));
}

static TC_PIV_result sm_transceive(TC_PIV_link* link, const TC_APDU_command* command,
                                   TC_buffer response, TC_APDU_response* out)
{
  command_layout layout;
  TC_PIV_result result = command_fits(link, command, &layout);
  if (result != TC_PIV_OK)
    return result;
  if (command_protect(link, command, &layout) != TC_OK)
    return session_lost(link, response, 0, TC_PIV_ERROR);
  /* The new Le is one byte 00 (footnote 22). */
  const TC_APDU_command outer = {{link->sm_scratch, layout.field_length},
                                 TC_APDU_SHORT_MAX_NE,
                                 SM_CLA,
                                 command->ins,
                                 command->p1,
                                 command->p2};
  TC_APDU_response answer;
  const TC_APDU_result sent =
      tc_apdu_transceive_format(&link->channel, TC_APDU_SHORT, &outer, response, &answer);
  TC_secure_zero(link->sm_scratch, layout.field_length);
  if (sent != TC_APDU_OK)
    return session_lost(link, response, 0, tc_piv_channel_result(sent));
  /* Footnote 25: any outer status other than 9000 is an SM error. 61XX was
   * followed by the channel. */
  if (answer.sw != TC_PIV_SW_SUCCESS_VALUE)
    return session_lost(link, response, answer.sw, TC_PIV_CARD_STATUS);
  response_fields fields;
  uint8_t* plaintext = NULL;
  size_t plain_length = 0;
  if (!response_read(answer.data, command->ins, &fields) ||
      response_unprotect(link, response, &fields, &plaintext, &plain_length) != TC_OK)
    return session_lost(link, response, 0, TC_PIV_INVALID);
  out->data = (TC_bytes){plaintext, plain_length};
  out->sw = fields.sw;
  return TC_PIV_OK;
}

const struct tc_piv_link_security tc_piv_sm_security = {sm_transceive, sm_unbind};

TC_PIV_result TC_PIV_link_secure(TC_PIV_link* link, TC_PIV_SM_workspace* workspace,
                                 TC_buffer sm_scratch)
{
  if (!tc_piv_link_ready(link) || !workspace || !sm_scratch.data ||
      sm_scratch.capacity < TC_PIV_SM_COMMAND_DATA_BYTES(TC_PIV_COMMAND_MAX_NC) ||
      link->security != &tc_piv_sm_security || (link->flags & TC_PIV_LINK_SECURED) ||
      TC_PIV_SM_get_state((const TC_PIV_SM*)link->sm) != TC_PIV_SM_READY ||
      !tc_piv_link_disjoint(link, sm_scratch.data, sm_scratch.capacity) ||
      !tc_piv_link_disjoint(link, workspace, sizeof *workspace) ||
      !tc_internal_ranges_disjoint(sm_scratch.data, sm_scratch.capacity, workspace,
                                   sizeof *workspace))
    return TC_PIV_ARGUMENT;
  link->sm_workspace = workspace;
  link->sm_scratch = sm_scratch.data;
  link->sm_scratch_capacity = sm_scratch.capacity;
  link->flags |= TC_PIV_LINK_SECURED;
  return TC_PIV_OK;
}

int TC_PIV_link_sm_peer_matches(const TC_PIV_link* link, TC_bytes certificate)
{
  return link && (link->flags & TC_PIV_LINK_SECURED) && !(link->flags & TC_PIV_LINK_SM_LOST) &&
         link->security == &tc_piv_sm_security && link->sm &&
         TC_PIV_SM_peer_matches((const TC_PIV_SM*)link->sm, certificate);
}

void TC_PIV_link_unsecure(TC_PIV_link* link)
{
  if (!link)
    return;
  tc_piv_link_unbind(link);
  link->flags &= (uint8_t)~(TC_PIV_LINK_SM_LOST | TC_PIV_LINK_VCI);
}
#endif
