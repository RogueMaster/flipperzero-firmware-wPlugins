/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_pcsc.h"
#include <string.h>

int example_card_pcsc_close(ExampleCardPCSC* state)
{
  int ok = 1;
  if (!state) return 0;
  if (state->transaction && SCardEndTransaction(state->card,SCARD_LEAVE_CARD) != SCARD_S_SUCCESS)
    ok = 0;
  if (state->connected && SCardDisconnect(state->card,SCARD_LEAVE_CARD) != SCARD_S_SUCCESS)
    ok = 0;
  if (state->established && SCardReleaseContext(state->context) != SCARD_S_SUCCESS)
    ok = 0;
  memset(state,0,sizeof *state);
  return ok;
}

int example_card_pcsc_open(ExampleCardPCSC* state, const char* reader)
{
  if (!state || !reader || !*reader || state->established || state->connected || state->transaction)
    return 0;
  if (SCardEstablishContext(SCARD_SCOPE_SYSTEM,NULL,NULL,&state->context) != SCARD_S_SUCCESS)
    goto failed;
  state->established = 1;
#if defined(_WIN32)
  if (SCardConnectA(state->context,reader,SCARD_SHARE_EXCLUSIVE,
#else
  if (SCardConnect(state->context,reader,SCARD_SHARE_EXCLUSIVE,
#endif
      SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,&state->card,&state->protocol) != SCARD_S_SUCCESS)
    goto failed;
  state->connected = 1;
  if (state->protocol != SCARD_PROTOCOL_T0 && state->protocol != SCARD_PROTOCOL_T1) goto failed;
  if (SCardBeginTransaction(state->card) != SCARD_S_SUCCESS) goto failed;
  state->transaction = 1;
  state->failed = 0;
  return 1;
failed:
  (void)example_card_pcsc_close(state);
  return 0;
}

int example_card_pcsc_transmit(void* context, const uint8_t* command,
    size_t command_length, uint8_t* response, size_t capacity, size_t* length)
{
  ExampleCardPCSC* state = context;
  const size_t max_transfer = (ExamplePCSCSize)-1;
  if (!state || !state->transaction || state->failed || !command || !command_length ||
      !response || !length || capacity < EXAMPLE_CARD_STATUS_BYTES ||
      command_length > max_transfer || capacity > max_transfer) return 0;
  ExamplePCSCSize received = (ExamplePCSCSize)capacity;
  const SCARD_IO_REQUEST* protocol = state->protocol == SCARD_PROTOCOL_T0 ? SCARD_PCI_T0 : SCARD_PCI_T1;
  if (SCardTransmit(state->card,protocol,command,(ExamplePCSCSize)command_length,
      NULL,response,&received) != SCARD_S_SUCCESS ||
      received < EXAMPLE_CARD_STATUS_BYTES || received > capacity) {
    state->failed = 1;
    TC_secure_zero(response,capacity);
    return 0;
  }
  *length = received;
  return 1;
}
