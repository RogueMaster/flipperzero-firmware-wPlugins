#ifndef NFC_TOOLS_MFC_H
#define NFC_TOOLS_MFC_H

#include "nfc_tools_i.h"
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── NDEF Read ─────────────────────────────────────────────────────────────────
// Reads the MAD, locates the NDEF sectors (AID 0x03E1), assembles the TLV buffer
// and populates app->ndef_str + app->ndef_records[]. Does nothing if inaccessible.
void nfc_tools_mfc_read_ndef(NfcToolsApp* app);

// ── NDEF Write ────────────────────────────────────────────────────────────────
// Initialises the MAD, writes the NDEF TLV to the card (1K or 4K).
// Returns true on success, false otherwise (app->info_str contains the error).
bool nfc_tools_mfc_write_ndef(NfcToolsApp* app, const uint8_t* ndef_data, size_t ndef_size);

// ── Format ────────────────────────────────────────────────────────────────────
// Erases all accessible data blocks and restores factory trailer values.
// Returns true if at least one sector was formatted.
// app->info_str contains the result message.
bool nfc_tools_mfc_format(NfcToolsApp* app);

// ── Memory Dump ───────────────────────────────────────────────────────────────
// Reads all blocks of the tag and fills app->info_str with the raw hex dump
// (one line of 16 bytes per block).
void nfc_tools_mfc_dump(NfcToolsApp* app);

#ifdef __cplusplus
}
#endif

#endif /* NFC_TOOLS_MFC_H */
