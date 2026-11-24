#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "url_links.h"

typedef struct GtaUrlNfc GtaUrlNfc;
/* Encodes a single NFC Forum URI record inside a Type 2 Tag NDEF TLV. */
size_t gta_url_ndef_encode(const char* url, uint8_t* out, size_t capacity);
GtaUrlNfc* gta_url_nfc_start(const char* url, uint16_t id, GtaUrlKind kind);
void gta_url_nfc_stop(GtaUrlNfc* session);
