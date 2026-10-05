/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * eMRTD - a reader for electronic identity documents, for the Flipper Zero.
 *
 * Reads the contactless chip of an ICAO Doc 9303 document - a passport, an
 * identity card, a residence permit - over NFC, opening it with PACE or BAC,
 * and exports every data group it can reach to the SD card.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Stamped into the APDU trace, so that a log says which package produced it. */
#define EMRTD_VERSION "1.0.0"

typedef struct Emrtd Emrtd;

#ifdef __cplusplus
}
#endif
