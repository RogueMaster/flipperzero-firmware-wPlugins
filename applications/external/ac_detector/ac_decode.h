#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ac_protocol_db.h"

/// The worker never hands us more than this, see MAX_TIMINGS_AMOUNT.
#define AC_MAX_TIMINGS 1024

/// 1024 timings is at most 512 bits, and section padding never adds more
/// than a byte per section.
#define AC_MAX_BYTES 72

/// "AA BB CC ..." for AC_MAX_BYTES plus the terminator.
#define AC_HEX_STR_LEN 224

/// Shortest capture we will look at. The smallest air conditioner frame we
/// know of is LG's 28 bits, which is 2 header + 56 bit + 1 stop timings.
#define AC_MIN_TIMINGS 48

/// Absolute floor for "this space is a gap, not a data bit", used by the noise
/// gate. The per-protocol parser uses a relative threshold instead - see
/// ac_parse() - because Samsung's inter-section gap is shorter than some other
/// protocols' one-space.
#define AC_SECTION_GAP_US 4000

typedef enum {
    /// Not a remote control frame. Lamp flicker, a stray reflection, a
    /// truncated capture. The display keeps whatever it showed before.
    AcResultNoise = 0,
    /// Structured like a remote, but nothing in the database fits.
    AcResultUnknown,
    /// Identified.
    AcResultMatch,
} AcResultKind;

typedef struct {
    AcResultKind kind;

    /// Only set when kind == AcResultMatch.
    const AcProtoEntry* entry;

    /// True bit count on the wire across every section, filler bits included,
    /// and across every repeat.
    uint16_t bits;
    /// How many times the frame was repeated inside one capture. 1 when the
    /// remote sent it once.
    uint8_t repeats;
    /// Headers seen. Multi-section protocols such as Daikin send several.
    uint8_t sections;
    /// Set when the entry's signature bytes were found, which is a much
    /// stronger claim than a timing match alone.
    bool signature_ok;

    /// As measured, so the user can compare against the database.
    uint16_t hdr_mark;
    uint16_t hdr_space;
    uint16_t bit_mark;
    uint16_t one_space;
    uint16_t zero_space;

    /// One frame's payload, byte aligned at every section boundary.
    uint8_t data[AC_MAX_BYTES];
    uint16_t data_len;

    uint16_t timings_count;
    uint32_t duration_us;
} AcDetection;

/// Classify one raw capture.
///
/// Returns true when the caller should replace what is on screen, which is
/// the case for AcResultUnknown and AcResultMatch but not for AcResultNoise.
bool ac_decode(const uint32_t* timings, size_t count, AcDetection* out);

/// "A5 3C 00 ..." into out.
void ac_detection_format_hex(const AcDetection* d, char* out, size_t len);
