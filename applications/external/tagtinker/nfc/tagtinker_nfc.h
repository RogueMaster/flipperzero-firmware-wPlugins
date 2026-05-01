/*
 * TagTinker — ESL NFC tag decoder
 *
 * Decodes NDEF URI from ESL Mifare Ultralight tags
 * into the 17-character barcode format used by TagTinker.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

bool tagtinker_nfc_decode_barcode(const MfUltralightData* mfu_data, char barcode[18]);
