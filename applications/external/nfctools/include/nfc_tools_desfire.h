#ifndef NFC_TOOLS_DESFIRE_H
#define NFC_TOOLS_DESFIRE_H

#include "nfc_tools_i.h"
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <toolbox/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── NDEF Write (NFC Forum Type 4 Tag) ────────────────────────────────────────
// Writes an NDEF message to a DESFire tag via ISO7816-4 APDUs (T=CL).
// If the NFC Forum application does not exist, attempts to create it (DESFire EV1+).
// Poller protocol used: NfcProtocolIso14443_4a (same path as nfc_commands_run).
//
// Returns true on success.
// app->info_str contains the result message (success or error).
bool nfc_tools_desfire_write_ndef(
    NfcToolsApp*   app,
    const uint8_t* ndef_data,
    size_t         ndef_size);

// ── NDEF Read (NFC Forum Type 4 Tag) ─────────────────────────────────────────
// Reads the NDEF content of a DESFire tag via T4T APDUs (ISO14443-4A poller).
// Populates app->ndef_str and app->ndef_records[].
// Returns true if at least the CC could be read (even if the message is empty).
bool nfc_tools_desfire_read_ndef(NfcToolsApp* app);

#ifdef __cplusplus
}
#endif

#endif /* NFC_TOOLS_DESFIRE_H */
