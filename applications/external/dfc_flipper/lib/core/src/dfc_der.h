#pragma once

// DER codec for the DFC v3 binary encoding (.dfcb).
//
// The encoding is the DER representation of a DFC credential.
// ASN.1 DER with implicit context tags. This is the emulator's native input on
// every platform. Text (.dfc) is an authoring format that is compiled to these
// octets before the emulator sees it, so the device runtime needs no text
// parser.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_credential.h"

// Largest .dfcb value, from section 2.2.1. Bounds every length to two octets.
#define DFC_DER_MAX_SIZE 65535

// Error classes from section 2.4. Conformance is on the class, not the message.
typedef enum {
    DfcDerOk = 0,
    // Violates section 2.1 or 2.2: broken encoding, out-of-range value, or a
    // semantic constraint from section 2.2.4.
    DfcDerMalformed,
    // Well formed, but names something this build does not implement.
    DfcDerUnsupported,
    // Well formed and supported, but larger than this build can hold.
    DfcDerCapacity,
} DfcDerStatus;

// Human-readable name of a status, for logs and test failures.
const char* dfc_der_status_name(DfcDerStatus status);

// Encode `credential` into `out`. On success writes the length to `*len`.
// Returns DfcDerCapacity when `cap` or DFC_DER_MAX_SIZE is too small, and
// DfcDerMalformed when the model itself violates a section 2.2.4 rule, so a
// caller cannot emit a credential that a conforming decoder would reject.
DfcDerStatus
    dfc_der_encode(const DfcCredential* credential, uint8_t* out, size_t cap, size_t* len);

// Decode `in` into `credential`, which is cleared first. Enforces the section
// 2.2.1 subset and every section 2.2.4 semantic rule.
DfcDerStatus dfc_der_decode(DfcCredential* credential, const uint8_t* in, size_t len);

// Checks the model rules that both encodings share: generation gating, file
// consistency, and the bounds a field addresses. Returns DfcDerOk when the
// model is sound.
DfcDerStatus dfc_der_validate_model(const DfcCredential* credential);

// Size the encoding would occupy, without writing it. Returns DfcDerOk and the
// length, or the same failures as dfc_der_encode.
DfcDerStatus dfc_der_encoded_size(const DfcCredential* credential, size_t* len);
