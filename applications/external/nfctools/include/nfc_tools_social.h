#ifndef NFC_TOOLS_SOCIAL_H
#define NFC_TOOLS_SOCIAL_H

#include <stdint.h>

// Social network data — shared between the selection scene
// and the NDEF builder.

typedef struct {
    const char* name;
    const char* url_prefix; // part before the username
    const char* url_suffix; // part after (empty if USERNAME is at the end of the URL)
} NfcToolsSocialNetwork;

extern const NfcToolsSocialNetwork nfc_tools_social_networks[];
extern const uint8_t               nfc_tools_social_networks_count;

#endif /* NFC_TOOLS_SOCIAL_H */
