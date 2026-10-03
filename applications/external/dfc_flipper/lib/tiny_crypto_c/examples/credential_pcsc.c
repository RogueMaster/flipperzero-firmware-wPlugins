/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_pcsc.h"
#include <string.h>

#if defined(_WIN32)
#define EXAMPLE_SCARD_LIST_READERS SCardListReadersA
#define EXAMPLE_SCARD_STATUS_CHANGE SCardGetStatusChangeA
#define EXAMPLE_SCARD_CONNECT SCardConnectA
typedef SCARD_READERSTATEA ExamplePCSCReaderState;
#else
#define EXAMPLE_SCARD_LIST_READERS SCardListReaders
#define EXAMPLE_SCARD_STATUS_CHANGE SCardGetStatusChange
#define EXAMPLE_SCARD_CONNECT SCardConnect
typedef SCARD_READERSTATE ExamplePCSCReaderState;
#endif

/* Reader name list capacity: a NUL-separated list ending in an empty name. */
enum { READER_NAMES_BYTES = 4096 };

/* 1 when bytes contain word, ignoring ASCII letter case. word is lower case. */
static int contains_word(const uint8_t* bytes, size_t length, const char* word)
{
  const size_t word_length = strlen(word);
  for (size_t start = 0; start + word_length <= length; ++start) {
    size_t i = 0;
    while (i < word_length) {
      uint8_t value = bytes[start + i];
      if (value >= 'A' && value <= 'Z')
        value = (uint8_t)(value - 'A' + 'a');
      if (value != (uint8_t)word[i])
        break;
      ++i;
    }
    if (i == word_length)
      return 1;
  }
  return 0;
}

/* 1 for text that names a Yubico device. */
static int names_yubico(const uint8_t* bytes, size_t length)
{
  return contains_word(bytes, length, "yubico") || contains_word(bytes, length, "yubikey");
}

/* PC/SC Part 3 section 3.1.3.2.3: a contactless card behind a PC/SC reader
 * answers with the synthesized ATR 3B 8X 80 01. */
static int contactless_atr(const uint8_t* atr, size_t length)
{
  return length >= 4 && atr[0] == 0x3b && (atr[1] & 0xf0) == 0x80 && atr[2] == 0x80 &&
         atr[3] == 0x01;
}

/* The one reader name that contains filter. names is a PC/SC multi-string of
 * length bytes. */
static ExampleCardPCSCResult reader_find(const char* names, size_t length, const char* filter,
                                         const char** out)
{
  const char* found = NULL;
  size_t matches = 0;
  for (size_t offset = 0; offset < length && names[offset];) {
    const char* name = names + offset;
    size_t name_length = 0;
    while (offset + name_length < length && name[name_length])
      ++name_length;
    if (offset + name_length == length)
      return EXAMPLE_PCSC_FAILED; /* an unterminated list */
    if (strstr(name, filter)) {
      found = name;
      ++matches;
    }
    offset += name_length + 1;
  }
  if (!matches)
    return EXAMPLE_PCSC_NO_READER;
  if (matches > 1)
    return EXAMPLE_PCSC_AMBIGUOUS;
  if (names_yubico((const uint8_t*)found, strlen(found)))
    return EXAMPLE_PCSC_REFUSED;
  *out = found;
  return EXAMPLE_PCSC_OPENED;
}

/* Read the ATR of reader without connecting and check it against the
 * request. */
static ExampleCardPCSCResult atr_check(ExampleCardPCSC* state, const char* reader,
                                       ExampleCardPCSCInterface interface)
{
  ExamplePCSCReaderState reader_state;
  memset(&reader_state, 0, sizeof reader_state);
  reader_state.szReader = reader;
  reader_state.dwCurrentState = SCARD_STATE_UNAWARE;
  if (EXAMPLE_SCARD_STATUS_CHANGE(state->context, 0, &reader_state, 1) != SCARD_S_SUCCESS ||
      reader_state.cbAtr > sizeof reader_state.rgbAtr)
    return EXAMPLE_PCSC_FAILED;
  if (!(reader_state.dwEventState & SCARD_STATE_PRESENT) || !reader_state.cbAtr)
    return EXAMPLE_PCSC_NO_CARD;
  if (names_yubico(reader_state.rgbAtr, reader_state.cbAtr))
    return EXAMPLE_PCSC_REFUSED;
  const int contactless = contactless_atr(reader_state.rgbAtr, reader_state.cbAtr);
  if (contactless && interface == EXAMPLE_PCSC_CONTACT)
    return EXAMPLE_PCSC_INTERFACE;
  state->interface =
      contactless || interface == EXAMPLE_PCSC_CONTACTLESS ? TC_PIV_CONTACTLESS : TC_PIV_CONTACT;
  return EXAMPLE_PCSC_OPENED;
}

int example_card_pcsc_close(ExampleCardPCSC* state)
{
  int ok = 1;
  if (!state)
    return 0;
  const ExamplePCSCSize disposition = state->reset ? SCARD_RESET_CARD : SCARD_LEAVE_CARD;
  if (state->transaction && SCardEndTransaction(state->card, SCARD_LEAVE_CARD) != SCARD_S_SUCCESS)
    ok = 0;
  if (state->connected && SCardDisconnect(state->card, disposition) != SCARD_S_SUCCESS)
    ok = 0;
  if (state->established && SCardReleaseContext(state->context) != SCARD_S_SUCCESS)
    ok = 0;
  memset(state, 0, sizeof *state);
  return ok;
}

ExampleCardPCSCResult example_card_pcsc_open(ExampleCardPCSC* state,
                                             const ExampleCardPCSCOptions* options)
{
  if (!state || !options || !options->reader || !*options->reader ||
      (unsigned)options->interface > EXAMPLE_PCSC_CONTACTLESS ||
      (options->guard && !options->guard->check) || state->established || state->connected ||
      state->transaction)
    return EXAMPLE_PCSC_FAILED;
  if (names_yubico((const uint8_t*)options->reader, strlen(options->reader)))
    return EXAMPLE_PCSC_REFUSED;
  static char names[READER_NAMES_BYTES];
  ExampleCardPCSCResult result = EXAMPLE_PCSC_FAILED;
  if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &state->context) != SCARD_S_SUCCESS)
    goto failed;
  state->established = 1;
  ExamplePCSCSize length = sizeof names;
  if (EXAMPLE_SCARD_LIST_READERS(state->context, NULL, names, &length) != SCARD_S_SUCCESS ||
      length > sizeof names)
    goto failed;
  const char* reader = NULL;
  result = reader_find(names, length, options->reader, &reader);
  if (result != EXAMPLE_PCSC_OPENED)
    goto failed;
  result = atr_check(state, reader, options->interface);
  if (result != EXAMPLE_PCSC_OPENED)
    goto failed;
  result = EXAMPLE_PCSC_FAILED;
  if (EXAMPLE_SCARD_CONNECT(state->context, reader, SCARD_SHARE_EXCLUSIVE,
                            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &state->card,
                            &state->protocol) != SCARD_S_SUCCESS)
    goto failed;
  state->connected = 1;
  if (state->protocol != SCARD_PROTOCOL_T0 && state->protocol != SCARD_PROTOCOL_T1)
    goto failed;
  if (SCardBeginTransaction(state->card) != SCARD_S_SUCCESS)
    goto failed;
  state->transaction = 1;
  state->failed = 0;
  state->guard = options->guard;
  memset(names, 0, sizeof names);
  if (state->guard && state->guard->connected)
    state->guard->connected(state->guard->context, state->interface);
  return EXAMPLE_PCSC_OPENED;
failed:
  memset(names, 0, sizeof names);
  (void)example_card_pcsc_close(state);
  return result;
}

TC_PIV_interface example_card_pcsc_interface(const ExampleCardPCSC* state)
{
  return state ? state->interface : TC_PIV_CONTACTLESS;
}

void example_card_pcsc_reset_on_close(ExampleCardPCSC* state)
{
  if (state)
    state->reset = 1;
}

TC_status example_card_pcsc_transmit(void* context, TC_bytes command, TC_buffer response,
                                     size_t* length)
{
  ExampleCardPCSC* state = context;
  const size_t max_transfer = (ExamplePCSCSize)-1;
  if (!state || !state->transaction || state->failed || !command.data || !command.length ||
      !response.data || !length || response.capacity < TC_APDU_STATUS_BYTES ||
      command.length > max_transfer || response.capacity > max_transfer)
    return TC_ERROR;
  const ExampleCardPCSCGuard* guard = state->guard;
  if (guard && !guard->check(guard->context, command)) {
    state->failed = 1;
    TC_secure_zero(response.data, response.capacity);
    return TC_ERROR;
  }
  ExamplePCSCSize received = (ExamplePCSCSize)response.capacity;
  const SCARD_IO_REQUEST* protocol =
      state->protocol == SCARD_PROTOCOL_T0 ? SCARD_PCI_T0 : SCARD_PCI_T1;
  if (SCardTransmit(state->card, protocol, command.data, (ExamplePCSCSize)command.length, NULL,
                    response.data, &received) != SCARD_S_SUCCESS ||
      received < TC_APDU_STATUS_BYTES || received > response.capacity) {
    state->failed = 1;
    TC_secure_zero(response.data, response.capacity);
    return TC_ERROR;
  }
  if (guard && guard->observe)
    guard->observe(guard->context, command, (TC_bytes){response.data, received});
  *length = received;
  return TC_OK;
}
