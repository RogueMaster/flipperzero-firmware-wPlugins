// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// helpers/oui_vendor.c -- the curated OUI -> vendor label shown on the WiFi
// detail screen. Small, but it is displayed next to a detection, so a wrong
// vendor is a wrong claim.
#include "test.h"
#include "oui_vendor.h"

#include <stddef.h>
#include <string.h>

void suite_oui_vendor(void);

void suite_oui_vendor(void) {
    printf("[oui_vendor]\n");

    // Known entry, and only the first 3 bytes participate in the match.
    const uint8_t esp[6] = {0x24, 0x0a, 0xc4, 0x11, 0x22, 0x33};
    const char* v = oui_vendor(esp);
    CHECK(v != NULL);
    CHECK_STR_EQ(v, "Espressif");

    const uint8_t esp2[6] = {0x24, 0x0a, 0xc4, 0xff, 0xff, 0xff};
    CHECK_STR_EQ(oui_vendor(esp2), "Espressif"); // trailing bytes are irrelevant

    // Unknown prefix returns NULL rather than a guess -- the detail screen shows
    // nothing instead of a wrong manufacturer.
    const uint8_t unknown[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CHECK(oui_vendor(unknown) == NULL);
    const uint8_t unknown2[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    CHECK(oui_vendor(unknown2) == NULL);

    // A near-miss must not match: one wrong byte in the OUI is a different vendor.
    const uint8_t near1[6] = {0x25, 0x0a, 0xc4, 0, 0, 0};
    const uint8_t near2[6] = {0x24, 0x0b, 0xc4, 0, 0, 0};
    const uint8_t near3[6] = {0x24, 0x0a, 0xc5, 0, 0, 0};
    CHECK(oui_vendor(near1) == NULL);
    CHECK(oui_vendor(near2) == NULL);
    CHECK(oui_vendor(near3) == NULL);

    // NULL in, NULL out (called with whatever a scan produced).
    CHECK(oui_vendor(NULL) == NULL);
}
