#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>
#include <nfc/protocols/nfc_protocol.h>

/* Warden's read layer.
 *
 * A worker thread drives the onboard NFC field as a *poller* (never a listener):
 *   1. NfcScanner sweeps every protocol and reports the full stack present on the
 *      card (e.g. Iso14443_3a -> Iso14443_4a -> MfDesfire).
 *   2. A short base-layer poll (ISO14443-3A / -3B / -15693 / FeliCa) pulls the
 *      activation data every access system keys on: the UID, plus SAK/ATQA on
 *      the A family. That is all Warden needs to grade the card.
 *
 * We only ever READ the anticollision/activation response. No sectors are
 * decrypted, no keys are exercised, nothing is written back. The card leaves in
 * exactly the state it arrived. */

#define WARDEN_UID_MAX 10u

typedef enum {
    CardReaderIdle, // not started
    CardReaderScanning, // field on, waiting for a card
    CardReaderReady, // a card was read; result is valid
    CardReaderError, // could not take over the NFC HAL
} CardReaderState;

typedef struct {
    NfcProtocol top; // most-derived protocol on the card (the "real" tech)
    NfcProtocol base; // root protocol of the stack (carries the UID)
    size_t protocol_num; // how many layers were detected
    NfcProtocol stack[NfcProtocolNum]; // full detected stack, base -> derived

    uint8_t uid[WARDEN_UID_MAX];
    size_t uid_len; // 0 if the UID could not be read
    bool has_iso3a; // SAK/ATQA below are valid (ISO14443-3A family)
    uint8_t sak;
    uint8_t atqa[2];
} CardReading;

typedef struct CardReader CardReader;

CardReader* card_reader_alloc(void);
void card_reader_free(CardReader* cr);

/* Arm the field and begin looking for a card (one-shot: stops on first read). */
void card_reader_start(CardReader* cr);
/* Drop the field and park the worker. Safe to call repeatedly. */
void card_reader_stop(CardReader* cr);

CardReaderState card_reader_state(CardReader* cr);

/* Copy the latest reading out for the UI/grader. Returns true if state==Ready. */
bool card_reader_get(CardReader* cr, CardReading* out);
