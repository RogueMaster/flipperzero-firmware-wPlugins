#pragma once

#include <stdbool.h>

/* Parses a "MAJOR.MINOR.PATCH" semantic version string into its three
 * integer components. Pure domain logic: no furi, no I/O — this is what
 * `make test` exercises on the host to prove the toolchain works end to end.
 *
 * Returns true and fills major/minor/patch on success. Returns false on any
 * malformed input (non-digit characters, missing/extra dot-separated
 * segments, empty segments, or a NULL argument) and leaves the outputs
 * untouched.
 */
bool gurpil_version_parse(const char *version, int *major, int *minor, int *patch);
