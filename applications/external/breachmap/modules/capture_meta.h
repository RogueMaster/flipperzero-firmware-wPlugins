#pragma once

#include "../models/recon_types.h"
#include <storage/storage.h>

/* Guess the evidence type from a file path's extension. */
EvidenceType capture_meta_type_from_path(const char* path);

/* Read a short human-readable metadata summary from a capture file:
 *  - .sub : frequency and preset/protocol
 *  - .nfc : device type and UID
 * Writes into "out_info" (up to out_len). Returns true if anything was parsed. */
bool capture_meta_extract(Storage* storage, const char* path, char* out_info, size_t out_len);
