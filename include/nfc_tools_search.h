#ifndef NFC_TOOLS_SEARCH_H
#define NFC_TOOLS_SEARCH_H

#include <stdint.h>

// Search engine data — shared between the selection scene
// and the NDEF builder.

typedef struct {
    const char* name;
    const char* url_prefix; // part before the keyword (already encoded)
    const char* url_suffix; // part after (empty except for Boardreader)
} NfcToolsSearchEngine;

extern const NfcToolsSearchEngine nfc_tools_search_engines[];
extern const uint8_t              nfc_tools_search_engines_count;

#endif /* NFC_TOOLS_SEARCH_H */
