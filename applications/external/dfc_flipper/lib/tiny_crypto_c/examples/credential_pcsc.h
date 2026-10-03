/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_PCSC_H_
#define EXAMPLE_CREDENTIAL_PCSC_H_
#include <tiny_crypto/piv_command.h>
#if defined(__APPLE__)
#include <PCSC/winscard.h>
typedef uint32_t ExamplePCSCSize;
#else
#include <winscard.h>
typedef DWORD ExamplePCSCSize;
#endif

/* Optional transmit guard, such as the hardware test guard. check sees each
 * command APDU before transmission and returns 1 to send it or 0 to refuse
 * it. observe, when set, sees each command with its complete answer. Both
 * run synchronously inside example_card_pcsc_transmit. connected, when set,
 * receives the interface of the connection once example_card_pcsc_open
 * connected, before any command. */
typedef struct {
  int (*check)(void* context, TC_bytes command);
  void (*observe)(void* context, TC_bytes command, TC_bytes answer);
  void* context;
  void (*connected)(void* context, TC_PIV_interface interface);
} ExampleCardPCSCGuard;

/* The interface the application expects. DETECT takes the interface the ATR
 * shows. */
typedef enum {
  EXAMPLE_PCSC_DETECT,
  EXAMPLE_PCSC_CONTACT,
  EXAMPLE_PCSC_CONTACTLESS
} ExampleCardPCSCInterface;

/* reader is a nonempty substring of exactly one reader name, compared as
 * bytes. guard may be NULL and must outlive the connection. */
typedef struct {
  const char* reader;
  ExampleCardPCSCInterface interface;
  const ExampleCardPCSCGuard* guard;
} ExampleCardPCSCOptions;

typedef enum {
  EXAMPLE_PCSC_OPENED,
  EXAMPLE_PCSC_NO_READER, /* no reader name contains the filter */
  EXAMPLE_PCSC_AMBIGUOUS, /* several reader names contain it */
  EXAMPLE_PCSC_REFUSED,   /* a Yubico reader name or a YubiKey ATR */
  EXAMPLE_PCSC_NO_CARD,   /* the reader holds no card */
  EXAMPLE_PCSC_INTERFACE, /* a contactless ATR with a contact request */
  EXAMPLE_PCSC_FAILED     /* bad arguments, an open state or a PC/SC failure */
} ExampleCardPCSCResult;

typedef struct {
  SCARDCONTEXT context;
  SCARDHANDLE card;
  ExamplePCSCSize protocol;
  const ExampleCardPCSCGuard* guard;
  TC_PIV_interface interface;
  int established, connected, transaction, failed, reset;
} ExampleCardPCSC;

/* Zero-initialize state, then open one reader. The open selects the reader
 * by options->reader and refuses reader names that contain "yubico" or
 * "yubikey" in any letter case. It reads the ATR before connecting and
 * refuses an ATR that contains "yubikey" in any letter case, so an attached
 * YubiKey receives no connect and no command. A PC/SC contactless ATR
 * (3B 8X 80 01, PC/SC Part 3 section 3.1.3.2.3) refuses a contact request.
 * Only EXAMPLE_PCSC_OPENED connects: exclusively, with a transaction held
 * until close. Every other result releases what it acquired. Reader names use
 * the platform's narrow PC/SC encoding. Concurrent access requires caller
 * locking. */
ExampleCardPCSCResult example_card_pcsc_open(ExampleCardPCSC* state,
                                             const ExampleCardPCSCOptions* options);
/* The interface of an open connection: contactless when requested or shown
 * by the ATR, else contact. */
TC_PIV_interface example_card_pcsc_interface(const ExampleCardPCSC* state);
/* Reset the card on close, which clears its PIN status. Call it once a PIN
 * reached the card. Accepts NULL. */
void example_card_pcsc_reset_on_close(ExampleCardPCSC* state);
/* Release every acquired resource. The card is left as it is unless it was
 * marked for reset. Returns zero if any release failed. State is cleared on
 * every close. */
int example_card_pcsc_close(ExampleCardPCSC* state);
/* A TC_APDU_transmit with state as its context. A failed transfer or a
 * command the guard refuses wipes the response buffer and disables further
 * transfers on this connection. No reconnect is attempted. */
TC_status example_card_pcsc_transmit(void* context, TC_bytes command, TC_buffer response,
                                     size_t* length);
#endif
