/**
 * Faraday - shielding grade engine.
 *
 * Pure, hardware-free scoring. Given a measured attenuation it returns a letter
 * grade, a one-word verdict and a plain-English line. Kept free of any Flipper
 * headers on purpose so it builds and unit-tests on the host (see test/).
 *
 * Two scales, because the two radios measure fundamentally different things:
 *   - Sub-GHz is a real power measurement, so we grade the drop in decibels.
 *   - NFC field-detect is a carrier-presence duty cycle, so we grade the
 *     percentage of the interrogation field the pouch keeps out.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Overall verdict, best to worst. Shared by both radios. */
typedef enum {
    FdyRatingAPlus = 0, /* lab-grade shielding      */
    FdyRatingA, /* excellent                */
    FdyRatingB, /* good                     */
    FdyRatingC, /* fair                     */
    FdyRatingD, /* weak                     */
    FdyRatingF, /* effectively no shielding */
    FdyRatingCount,
} FdyRating;

/* Lower bound of each grade, declared once.
 *
 * The grading engine below and every screen that explains the scale read
 * these same names, so a threshold change cannot leave the About text quietly
 * describing an older version of the app. */
#define FDY_DB_APLUS 60
#define FDY_DB_A 45
#define FDY_DB_B 30
#define FDY_DB_C 20
#define FDY_DB_D 10

#define FDY_PCT_APLUS 98
#define FDY_PCT_A 90
#define FDY_PCT_B 75
#define FDY_PCT_C 50
#define FDY_PCT_D 20

/** "A+", "A", ... "F" */
const char* fdy_rating_letter(FdyRating r);

/** One-word verdict: SEALED / STRONG / GOOD / FAIR / WEAK / OPEN */
const char* fdy_rating_word(FdyRating r);

/** A short plain-English sentence for the verdict screen / about copy. */
const char* fdy_rating_blurb(FdyRating r);

/** 0..5 filled pips, for a compact strength indicator. */
uint8_t fdy_rating_pips(FdyRating r);

/**
 * Grade a Sub-GHz shielding test by attenuation in dB
 * (baseline_dbm - shielded_dbm). Negative/near-zero => F.
 */
FdyRating fdy_grade_db(int16_t atten_db);

/**
 * Grade an NFC shielding test by the percentage of the reader field kept out
 * (0..100). 100 = no carrier detectable inside the pouch.
 */
FdyRating fdy_grade_pct(uint8_t blocked_pct);

#ifdef __cplusplus
}
#endif
