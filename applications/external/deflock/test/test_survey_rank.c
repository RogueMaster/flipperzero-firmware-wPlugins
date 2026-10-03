// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Survey ranking. The headline case is issue #25's real capture: the ranker has
// to pick out the same row a human picked out by hand, from the same data.
#include "survey_rank.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

// Convenience: build a row without repeating the brace soup.
static SurveyRankRow
    mk(uint8_t b0,
       uint8_t b1,
       uint8_t b2,
       uint8_t b3,
       uint8_t b4,
       uint8_t b5,
       int8_t rssi,
       uint8_t ch,
       uint32_t fp,
       uint16_t count) {
    SurveyRankRow r;
    memset(&r, 0, sizeof(r));
    r.mac[0] = b0;
    r.mac[1] = b1;
    r.mac[2] = b2;
    r.mac[3] = b3;
    r.mac[4] = b4;
    r.mac[5] = b5;
    r.rssi = rssi;
    r.channel = ch;
    r.fp = fp;
    r.count = count;
    return r;
}

void suite_survey_rank(void) {
    printf("[survey_rank]\n");

    // --- locally administered bit -------------------------------------------
    // Bit 1 of the first octet. This is why an OUI table finds nothing on a
    // modern camera, and it is reported as context, never scored.
    const uint8_t local[6] = {0x06, 0xFC, 0xCB, 0x3A, 0xF8, 0x9E};
    const uint8_t global[6] = {0xF8, 0xD2, 0xAC, 0xC5, 0x97, 0xAC};
    CHECK(survey_mac_is_local(local));
    CHECK(!survey_mac_is_local(global));
    CHECK(!survey_mac_is_local(NULL));
    // 0x7E and 0x7A both have bit 1 set; 0x04 does not.
    const uint8_t seven_e[6] = {0x7E, 0x68, 0xF0, 0x04, 0xF6, 0x6B};
    const uint8_t seven_a[6] = {0x7A, 0xB2, 0x1B, 0x2C, 0xF2, 0xAD};
    const uint8_t four[6] = {0x04, 0xA8, 0x5A, 0x00, 0x00, 0x01};
    CHECK(survey_mac_is_local(seven_e));
    CHECK(survey_mac_is_local(seven_a));
    CHECK(!survey_mac_is_local(four));

    // --- degenerate inputs ---------------------------------------------------
    SurveyRanked out[16];
    SurveyRankRow one = mk(0x06, 0, 0, 0, 0, 1, -40, 6, 0xaaaaaaaau, 5);
    CHECK_INT_EQ((int)survey_rank(NULL, 1, out, 16), 0);
    CHECK_INT_EQ((int)survey_rank(&one, 0, out, 16), 0);
    CHECK_INT_EQ((int)survey_rank(&one, 1, NULL, 16), 0);
    CHECK_INT_EQ((int)survey_rank(&one, 1, out, 0), 0);

    // --- ISSUE #25, the real capture, WITH THE CORRECTION ---------------------
    // Read straight from wiilover22's survey.csv, and this fixture used to assert
    // the wrong answer. 06:FC:CB:3A:F8:9E is the row he parked next to and the
    // row this project told him was his camera: -26 dBm, 10 probes, nothing else
    // in the file above 2. It looks like the find, and the ranker put it first.
    //
    // His 2026-09-11 drive retracted it. 0x89C3DEBF turned up on TEN devices
    // spread over 7.9 km at -17 to -96 dBm, so it is a commodity skeleton and is
    // now on the generic denylist -- which means THIS row, the closest and
    // busiest in the capture, must now sink to zero. That is the whole point of
    // the denylist and it is worth a test that hurts: the most compelling row by
    // eye is the one being thrown away.
    //
    // 7A:B2:1B carried 0xBA9FAFA0, which survived that round and was then itself
    // retracted on 2026-10-02 when the reporter watched it follow his car. The
    // row keeps its place in this fixture with an UNCLASSIFIED hash instead,
    // because the behaviour under test is the ranker, not that particular hash:
    // a close, busy, randomised address whose fingerprint is on no denylist is
    // what must rise. Do not put a real shipped hash here. Every one that has
    // ever been written into this fixture has later been retracted, and each
    // time the test went on asserting the retracted answer was correct.
    SurveyRankRow field[7] = {
        mk(0x06, 0xFC, 0xCB, 0x3A, 0xF8, 0x9E, -26, 6, 0x89c3debfu, 10), // RETRACTED
        mk(0x7E, 0x68, 0xF0, 0x04, 0xF6, 0x6B, -50, 1, 0x96fcd1b2u, 2), // generic
        mk(0x7E, 0x68, 0xF0, 0x8E, 0x5A, 0x48, -55, 6, 0x96fcd1b2u, 1), // generic
        mk(0x7E, 0x68, 0xF0, 0xB0, 0xCE, 0xFB, -60, 11, 0x96fcd1b2u, 1), // generic
        mk(0x7E, 0x68, 0xF0, 0x80, 0x2E, 0x5C, -62, 6, 0x96fcd1b2u, 1), // generic
        mk(0x7A, 0xB2, 0x1B, 0x2C, 0xF2, 0xAD, -37, 8, 0x5A17C3E2u, 2), // unclassified
        mk(0xF8, 0xD2, 0xAC, 0xC5, 0x97, 0xAC, -87, 6, 0xc4e51f77u, 1), // Vantiva, far
    };
    size_t n = survey_rank(field, 7, out, 16);
    CHECK_INT_EQ((int)n, 7);

    // 7A:B2:1B ranks first. This is the whole feature.
    CHECK_INT_EQ((int)out[0].index, 5);
    CHECK(out[0].score > out[1].score);
    CHECK(out[0].evidence & SurveyEvidenceLocalAdmin); // randomised, no OUI to match
    CHECK(out[0].evidence & SurveyEvidenceClose); // -37 dBm
    CHECK(!(out[0].evidence & SurveyEvidenceGeneric));
    // One sighting, one address: it has NOT been seen rotating, and claiming so
    // would invent evidence.
    CHECK(!(out[0].evidence & SurveyEvidenceRotating));

    // THE RETRACTED ROW SINKS, and it is the closest and busiest row in the file.
    // A denylisted skeleton scores nothing no matter how good the rest of its
    // evidence looks -- which is the behaviour that stops us handing an operator
    // a phone to go and look at, for the second time.
    size_t retracted = 0;
    for(size_t i = 0; i < n; i++) {
        if(out[i].index == 0) retracted = i;
    }
    CHECK(out[retracted].evidence & SurveyEvidenceGeneric);
    CHECK_INT_EQ(out[retracted].score, 0);

    // Every generic-skeleton row sits at the bottom at score 0: the four-address
    // 96fcd1b2 cluster that looks the most compelling by eye, plus 89c3debf.
    for(size_t i = 2; i < 7; i++) {
        CHECK(out[i].evidence & SurveyEvidenceGeneric);
        CHECK_INT_EQ(out[i].score, 0);
    }
    // ...and the distant neighbour's set-top box, on a real vendor OUI, still
    // outranks them: it is at least a device we can name.
    CHECK_INT_EQ((int)out[1].index, 6);
    CHECK(!(out[1].evidence & SurveyEvidenceLocalAdmin));

    // --- generic rows do not set the persistence scale ------------------------
    // A chatty phone running a stock scan must not flatten the real rows. Here
    // the generic row has 100 probes; the camera's 10 should still score full
    // persistence because the generic row is excluded from the scale.
    SurveyRankRow skew[2] = {
        mk(0x06, 0, 0, 0, 0, 1, -30, 6, 0x11111111u, 10),
        mk(0x02, 0, 0, 0, 0, 2, -30, 6, 0x96fcd1b2u, 100),
    };
    n = survey_rank(skew, 2, out, 16);
    CHECK_INT_EQ((int)n, 2);
    CHECK_INT_EQ((int)out[0].index, 0);
    CHECK(out[0].score >= 80); // full persistence + close
    CHECK_INT_EQ(out[1].score, 0);

    // --- rotation is scored only for non-generic fingerprints ----------------
    // Same fingerprint, two different addresses: that is the randomisation tell
    // a fingerprint exists to catch.
    SurveyRankRow rot[2] = {
        mk(0x06, 0, 0, 0, 0, 1, -70, 6, 0x33333333u, 4),
        mk(0x0A, 0, 0, 0, 0, 2, -70, 6, 0x33333333u, 4),
    };
    n = survey_rank(rot, 2, out, 16);
    CHECK_INT_EQ((int)n, 2);
    CHECK(out[0].evidence & SurveyEvidenceRotating);
    CHECK(out[1].evidence & SurveyEvidenceRotating);

    // The same MAC appearing twice is NOT rotation.
    SurveyRankRow same[2] = {
        mk(0x06, 0, 0, 0, 0, 1, -70, 6, 0x33333333u, 4),
        mk(0x06, 0, 0, 0, 0, 1, -70, 11, 0x33333333u, 4),
    };
    n = survey_rank(same, 2, out, 16);
    CHECK(!(out[0].evidence & SurveyEvidenceRotating));

    // A zero fingerprint is "none captured", not a shared one -- two rows with
    // no fingerprint are not evidence of one device rotating.
    SurveyRankRow nofp[2] = {
        mk(0x06, 0, 0, 0, 0, 1, -70, 6, 0, 4),
        mk(0x0A, 0, 0, 0, 0, 2, -70, 6, 0, 4),
    };
    n = survey_rank(nofp, 2, out, 16);
    CHECK(!(out[0].evidence & SurveyEvidenceRotating));
    CHECK(!(out[1].evidence & SurveyEvidenceRotating));

    // --- output cap is honoured ----------------------------------------------
    SurveyRanked small[2];
    n = survey_rank(field, 7, small, 2);
    CHECK_INT_EQ((int)n, 2);

    // --- signal floor ---------------------------------------------------------
    // At and below the floor, proximity contributes nothing; a lone row still
    // takes full persistence because it is the busiest thing present.
    SurveyRankRow faint[1] = {mk(0x06, 0, 0, 0, 0, 1, -95, 6, 0x44444444u, 1)};
    n = survey_rank(faint, 1, out, 16);
    CHECK_INT_EQ((int)n, 1);
    CHECK(!(out[0].evidence & SurveyEvidenceClose));
    CHECK(!(out[0].evidence & SurveyEvidencePersistent));
}
