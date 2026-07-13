#pragma once

#include <stdint.h>
#include <stdbool.h>

/* A minimal, read-only NFC front-end: sweep for a 13.56 MHz card and pull the
 * anticollision data every reader sees (UID / SAK / ATQA) plus the detected
 * technology name. This is exactly the exchange that happens *before* a Mifare
 * Classic Crypto1 authentication - which is why it pairs with the walkthrough.
 * It never authenticates, never writes, never runs a key-recovery attack. */

#define NFC_READER_UID_MAX 10

typedef enum {
    NfcReaderIdle,
    NfcReaderScanning,
    NfcReaderReady,
} NfcReaderState;

typedef struct {
    uint8_t uid[NFC_READER_UID_MAX];
    uint8_t uid_len;
    uint8_t sak;
    uint8_t atqa[2];
    bool has_iso3a;
    char tech[24]; // top protocol name, e.g. "Mifare Classic"
} NfcReading;

typedef struct NfcReader NfcReader;

NfcReader* nfc_reader_alloc(void);
void nfc_reader_free(NfcReader* r);

void nfc_reader_start(NfcReader* r); // spawn the scan worker
void nfc_reader_stop(NfcReader* r); // join + idle

NfcReaderState nfc_reader_state(NfcReader* r);
bool nfc_reader_get(NfcReader* r, NfcReading* out); // true only when Ready
