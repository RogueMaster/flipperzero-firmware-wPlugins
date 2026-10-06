#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <nfc/nfc_scanner.h>

typedef struct {
    bool iso14443a;
    bool iso15693;
    bool type2;
} MToolsCardProtocols;

/** Classify a scanner result once for all NFC tools. */
MToolsCardProtocols mtools_card_protocols(const NfcScannerEvent* event);
const char* mtools_iso15_chip_name(const uint8_t* uid, size_t uid_len);
void mtools_uid_format_hex(char* output, size_t size, const uint8_t* uid, size_t uid_len);
