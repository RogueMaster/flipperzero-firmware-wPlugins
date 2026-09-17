#pragma once

// Text codec for the DFC v3 authoring encoding (.dfc).
//
// The encoding is the line-oriented representation of a DFC credential.
// "Key: Value", LF terminated, deterministic field order. This codec works on a
// plain char buffer and depends on nothing but the credential model, so the same
// code runs in the host tests and on the device.
//
// Text is an authoring format. The emulator's input is always the binary
// encoding, so the load path compiles text to octets (dfc_der_encode) and reads
// the model back from those octets before anything is emulated.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_credential.h"
#include "dfc_der.h"

// Largest .dfc value this codec will handle. Text is an order of magnitude
// looser than the octets it compiles to, so this is generous rather than tight.
#define DFC_TEXT_MAX_SIZE 65536

// One error vocabulary for both encodings: the classes of section 2.4.
typedef DfcDerStatus DfcTextStatus;

#define DfcTextOk          DfcDerOk
#define DfcTextMalformed   DfcDerMalformed
#define DfcTextUnsupported DfcDerUnsupported
#define DfcTextCapacity    DfcDerCapacity

#define DFC_TEXT_ERROR_MESSAGE_MAX 96

// Where a parse failed. `line` is 1-based and 0 when the fault belongs to the
// document rather than to one line, which is what a person authoring a .dfc by
// hand needs in order to find it.
typedef struct {
    size_t line;
    char message[DFC_TEXT_ERROR_MESSAGE_MAX];
} DfcTextError;

const char* dfc_text_status_name(DfcTextStatus status);

// Parse `len` octets of `text` into `credential`, which is cleared first.
// `detail` may be NULL. Enforces section 2.1 in full plus the section 2.2.4
// semantic rules, so text this accepts is text the binary codec also accepts.
DfcTextStatus
    dfc_text_parse(DfcCredential* credential, const char* text, size_t len, DfcTextError* detail);

// Write the canonical text of `credential` into `out`, which is NUL-terminated
// when there is room for the terminator. `*len` receives the octet count the
// text occupies, excluding that terminator, whether or not it fit. Returns
// DfcTextCapacity when `out` is too small and DfcTextMalformed when the model
// itself is not representable. Passing NULL and a `cap` of 0 sizes the text
// without writing it.
DfcTextStatus dfc_text_write(const DfcCredential* credential, char* out, size_t cap, size_t* len);
