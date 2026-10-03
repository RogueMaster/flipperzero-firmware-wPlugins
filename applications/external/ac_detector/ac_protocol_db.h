#pragma once

#include <stdint.h>
#include <stddef.h>

/// How an entry confirms that a decoded payload really belongs to it.
///
/// Timings alone are not always enough. Coolix and Midea, for instance, use
/// the same line code and the same number of bits; only the payload tells
/// them apart.
typedef enum {
    AcSigNone = 0, ///< Trust the timings and the bit count
    AcSigPrefix, ///< Leading bytes match sig[] under sig_mask[]
    AcSigCoolix, ///< Three byte pairs, each second byte the complement
    AcSigMidea, ///< Six bytes, then the same six inverted, under a 0b10100 header
    AcSigBytePairs, ///< Every second byte is the inverse of the one before it
    AcSigNeoclima, ///< 0xA5 marker at byte 10 and a byte sum at byte 11
} AcSigKind;

/// One row of the protocol database.
typedef struct {
    const char* name; ///< "Coolix", "Daikin216", ...
    const char* variant; ///< Remote family, or "" when there is only one
    const char* brands; ///< Comma separated, tested brands first
    const char* app; ///< Matching remote app in this repo, or "-"

    // Some remotes send a short burst before the header proper. Zero when
    // the protocol has none.
    uint16_t lead_mark;
    uint16_t lead_space;

    uint16_t hdr_mark;
    uint16_t hdr_space;

    uint16_t bit_mark;
    uint16_t one_space;
    uint16_t zero_space;

    /// Data bits on the wire, filler bits between sections included. This is
    /// not always eight times the state length: Gree carries 8 state bytes
    /// but puts three constant bits in the middle, so it is 67.
    uint16_t bits;

    /// Set for general-purpose consumer protocols that are not air
    /// conditioner protocols at all. Some cheap portable and window units use
    /// a plain button-per-code remote instead of a full-state frame, and
    /// saying so is far more useful than reporting Unknown.
    uint8_t consumer;

    uint8_t msb_first; ///< Bit order within each byte
    uint8_t sig_kind; ///< AcSigKind
    uint8_t sig_len; ///< Bytes of sig[] in use
    uint8_t sig[6];
    uint8_t sig_mask[6];
} AcProtoEntry;

extern const AcProtoEntry ac_proto_db[];
extern const size_t ac_proto_db_count;
