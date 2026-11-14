#pragma once

#include <stdint.h>
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* iso15693_info_get_manufacturer_name(uint8_t vendor_id);

// Richer chip decode that inspects the whole UID (MSB-first, uid[0]==0xE0). For NXP I-Code parts
// it distinguishes SLI / SLIX / SLIX2 (and the -S / -L variants) via the type-indicator bits of
// uid[3]. For everything else it falls back to a manufacturer + IC-id table lookup.
const char* iso15693_info_get_chip_info_ex(const uint8_t* uid);

// The one UID formatter, so the byte layout is a single decision instead of one per screen. Both
// policies are layout choices and they are NOT interchangeable -- the widths are the reason:
typedef enum {
    Iso15693UidFormatSpaced, // "E0 04 01 50 12 34 56 78" -- 23 chars, so it needs a line of its own
    Iso15693UidFormatGrouped, // "E0040150 12345678" -- 17 chars, and fits beside other text on a
        // 128px line, which the spaced form overruns
} Iso15693UidFormat;

// Appends the UID -- ISO15693_3_UID_SIZE bytes, MSB-first -- and nothing else. Any label, separator or
// trailing newline is the caller's, so a caller wanting "UID: " must include its own space.
void iso15693_info_cat_uid(FuriString* out, const uint8_t* uid, Iso15693UidFormat format);

#ifdef __cplusplus
}
#endif
