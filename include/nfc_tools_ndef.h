#ifndef NFC_TOOLS_NDEF_H
#define NFC_TOOLS_NDEF_H

#include "nfc_tools_i.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── NDEF Parse ────────────────────────────────────────────────────────────────
// Parses the TLV stream of a Type 2 tag (NTAG / Ultralight / ICODE) and fills
// out with a human-readable representation.
void nfc_tools_ndef_parse_type2_tag(const uint8_t* data, size_t len, FuriString* out);

// Populates app->ndef_records[] from the same TLV stream.
void nfc_tools_ndef_parse_type2_tag_structured(
    NfcToolsApp* app,
    const uint8_t* data,
    size_t len);

// ── NDEF Build ────────────────────────────────────────────────────────────────
// Builds a raw NDEF message from app->ndef_buf1..6 according to app->ndef_type.
// Returns an allocated buffer (caller must free it) or NULL.
uint8_t* nfc_tools_ndef_build(NfcToolsApp* app, size_t* out_size);

// Returns the popup label for the given write type.
const char* nfc_tools_ndef_write_label(NdefType type);

#ifdef __cplusplus
}
#endif

#endif /* NFC_TOOLS_NDEF_H */
