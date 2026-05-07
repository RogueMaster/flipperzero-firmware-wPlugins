#ifndef NFC_TOOLS_NTAG_H
#define NFC_TOOLS_NTAG_H

#include "nfc_tools_i.h"
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Format ────────────────────────────────────────────────────────────────────
// Determines the exact type (GET_VERSION), computes the page range to erase,
// writes 0x00 to all user pages.
// Returns true on success. app->info_str contains the result.
bool nfc_tools_ntag_format(NfcToolsApp* app);

// ── Memory Dump ───────────────────────────────────────────────────────────────
// Reads all accessible pages and fills app->info_str (hex) and
// app->ndef_str (ASCII).
// Returns true on success.
bool nfc_tools_ntag_dump(NfcToolsApp* app);

#ifdef __cplusplus
}
#endif

#endif /* NFC_TOOLS_NTAG_H */
