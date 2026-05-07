#ifndef NFC_TOOLS_ICODE_H
#define NFC_TOOLS_ICODE_H

#include "nfc_tools_i.h"
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <nfc/protocols/slix/slix_poller.h>
#include <nfc/protocols/slix/slix.h>
#include <toolbox/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── NDEF Write ────────────────────────────────────────────────────────────────
// Writes an NDEF message to an ISO15693 / SLIX tag via the provided NfcPoller.
// use_slix=true for the SLIX channel, false for plain ISO15693-3.
// Returns true on success.
bool nfc_tools_icode_write_ndef(
    NfcToolsApp* app,
    const uint8_t* ndef_data,
    size_t ndef_size,
    bool use_slix);

// ── Format ────────────────────────────────────────────────────────────────────
// Erases all accessible blocks (write 0x00).
// Returns true on success. app->info_str contains the result.
bool nfc_tools_icode_format(NfcToolsApp* app, bool use_slix);

// ── Memory Dump ───────────────────────────────────────────────────────────────
// Reads all blocks and fills app->info_str (hex) and app->ndef_str (ASCII).
// Returns true on success.
bool nfc_tools_icode_dump(NfcToolsApp* app);

#ifdef __cplusplus
}
#endif

#endif /* NFC_TOOLS_ICODE_H */
