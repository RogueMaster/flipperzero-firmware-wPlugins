#pragma once

#include "seos_credential.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parsing a saved credential from an already-open file.
 *
 * Separate from file opening, dialogs and the loading callback so it can run
 * on a workstation and be covered by tests.
 */

/* Fills `credential` from an open file.
 *
 * False if the header is not recognised, a stated length exceeds the field it
 * is for, or a required field is missing. */
bool seos_credential_parse_seos(FlipperFormat* file, SeosCredential* credential);

/* Same, for the other tool's file format. */
bool seos_credential_parse_seader(FlipperFormat* file, SeosCredential* credential);

#ifdef __cplusplus
}
#endif
