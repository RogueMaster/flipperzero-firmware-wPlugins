/*
 * Host tests for the Faraday grading engine.
 *
 * The letter grade is what the user actually walks away with, so every band
 * boundary is pinned here. Builds the real helpers/fdy_grade.c - no stubs, no
 * copy of the logic - so a threshold edit that changes a verdict fails CI.
 *
 *   make -C test
 */
#include "helpers/fdy_grade.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void expect_db(int16_t db, FdyRating want) {
    checks++;
    FdyRating got = fdy_grade_db(db);
    if(got != want) {
        failures++;
        printf(
            "  FAIL  %4d dB -> %-2s (expected %s)\n",
            db,
            fdy_rating_letter(got),
            fdy_rating_letter(want));
    }
}

static void expect_pct(uint8_t pct, FdyRating want) {
    checks++;
    FdyRating got = fdy_grade_pct(pct);
    if(got != want) {
        failures++;
        printf(
            "  FAIL  %3u%% -> %-2s (expected %s)\n",
            pct,
            fdy_rating_letter(got),
            fdy_rating_letter(want));
    }
}

static void expect_str(const char* got, const char* want, const char* what) {
    checks++;
    if(strcmp(got, want) != 0) {
        failures++;
        printf("  FAIL  %s: got \"%s\", expected \"%s\"\n", what, got, want);
    }
}

static void expect_int(int got, int want, const char* what) {
    checks++;
    if(got != want) {
        failures++;
        printf("  FAIL  %s: got %d, expected %d\n", what, got, want);
    }
}

int main(void) {
    printf("Faraday grade engine\n");

    /* --- dB scale: each band, and both sides of every boundary --- */
    printf("- sub-ghz dB thresholds\n");
    expect_db(120, FdyRatingAPlus);
    expect_db(61, FdyRatingAPlus);
    expect_db(60, FdyRatingAPlus); /* boundary */
    expect_db(59, FdyRatingA);
    expect_db(45, FdyRatingA); /* boundary */
    expect_db(44, FdyRatingB);
    expect_db(30, FdyRatingB); /* boundary */
    expect_db(29, FdyRatingC);
    expect_db(20, FdyRatingC); /* boundary */
    expect_db(19, FdyRatingD);
    expect_db(10, FdyRatingD); /* boundary */
    expect_db(9, FdyRatingF);
    expect_db(0, FdyRatingF);

    /* A pouch that somehow reads louder than open air is still a failure, not
     * a crash or a wrapped-around grade. */
    printf("- sub-ghz negative attenuation\n");
    expect_db(-1, FdyRatingF);
    expect_db(-40, FdyRatingF);

    /* --- percentage scale --- */
    printf("- nfc %% thresholds\n");
    expect_pct(100, FdyRatingAPlus);
    expect_pct(98, FdyRatingAPlus); /* boundary */
    expect_pct(97, FdyRatingA);
    expect_pct(90, FdyRatingA); /* boundary */
    expect_pct(89, FdyRatingB);
    expect_pct(75, FdyRatingB); /* boundary */
    expect_pct(74, FdyRatingC);
    expect_pct(50, FdyRatingC); /* boundary */
    expect_pct(49, FdyRatingD);
    expect_pct(20, FdyRatingD); /* boundary */
    expect_pct(19, FdyRatingF);
    expect_pct(0, FdyRatingF);

    /* Out-of-range input clamps instead of indexing off the end. */
    printf("- nfc %% clamping\n");
    expect_pct(200, FdyRatingAPlus);

    /* --- labels --- */
    printf("- labels\n");
    expect_str(fdy_rating_letter(FdyRatingAPlus), "A+", "letter A+");
    expect_str(fdy_rating_letter(FdyRatingF), "F", "letter F");
    expect_str(fdy_rating_word(FdyRatingAPlus), "SEALED", "word A+");
    expect_str(fdy_rating_word(FdyRatingF), "OPEN", "word F");

    /* Every rating must have non-empty copy and a sane pip count. */
    for(int r = 0; r < FdyRatingCount; r++) {
        checks++;
        if(fdy_rating_letter((FdyRating)r)[0] == '\0' ||
           fdy_rating_word((FdyRating)r)[0] == '\0' || fdy_rating_blurb((FdyRating)r)[0] == '\0') {
            failures++;
            printf("  FAIL  rating %d has empty copy\n", r);
        }
        checks++;
        if(fdy_rating_pips((FdyRating)r) > 5) {
            failures++;
            printf("  FAIL  rating %d pips out of range\n", r);
        }
    }

    /* Pips must decrease monotonically as the grade gets worse. */
    printf("- pips ordering\n");
    expect_int(fdy_rating_pips(FdyRatingAPlus), 5, "pips A+");
    expect_int(fdy_rating_pips(FdyRatingF), 0, "pips F");
    for(int r = 1; r < FdyRatingCount; r++) {
        checks++;
        if(fdy_rating_pips((FdyRating)r) >= fdy_rating_pips((FdyRating)(r - 1))) {
            failures++;
            printf("  FAIL  pips not decreasing at rating %d\n", r);
        }
    }

    /* Out-of-range rating ids must not read past the tables. */
    printf("- rating id clamping\n");
    expect_str(fdy_rating_letter((FdyRating)99), "F", "letter clamp");
    expect_str(fdy_rating_word((FdyRating)99), "OPEN", "word clamp");
    expect_int(fdy_rating_pips((FdyRating)99), 0, "pips clamp");

    printf("\n%d checks, %d failures\n", checks, failures);
    if(failures == 0) printf("OK\n");
    return failures == 0 ? 0 : 1;
}
