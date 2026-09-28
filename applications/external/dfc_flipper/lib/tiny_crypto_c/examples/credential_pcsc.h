/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_PCSC_H_
#define EXAMPLE_CREDENTIAL_PCSC_H_
#include "credential_io.h"
#if defined(__APPLE__)
#include <PCSC/winscard.h>
typedef uint32_t ExamplePCSCSize;
#else
#include <winscard.h>
typedef DWORD ExamplePCSCSize;
#endif

typedef struct {
  SCARDCONTEXT context;
  SCARDHANDLE card;
  ExamplePCSCSize protocol;
  int established, connected, transaction, failed;
} ExampleCardPCSC;

/* Zero-initialize state. Open the named reader exclusively and hold a transaction
 * until close. A failed open releases acquired resources. Reader names use the
 * platform's narrow PC/SC encoding. Concurrent access requires caller locking. */
int example_card_pcsc_open(ExampleCardPCSC* state, const char* reader);
/* Release every acquired resource, leaving card contents and PIN unchanged.
 * Returns zero if any release failed. State is cleared on every close. */
int example_card_pcsc_close(ExampleCardPCSC* state);
/* Use as ExampleCardIO.transmit with state as its context. A failed transfer
 * disables further transfers on this connection. No reconnect is attempted. */
int example_card_pcsc_transmit(void* context, const uint8_t* command,
    size_t command_length, uint8_t* response, size_t capacity, size_t* length);
#endif
