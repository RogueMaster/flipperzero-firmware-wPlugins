#include "fdy_grade.h"

static const char* const LETTER[FdyRatingCount] = {"A+", "A", "B", "C", "D", "F"};
static const char* const WORD[FdyRatingCount] =
    {"SEALED", "STRONG", "GOOD", "FAIR", "WEAK", "OPEN"};
/* One line each, and SHORT on purpose: these are rendered on a 128 px screen
 * whose text element wraps on width rather than on word boundaries, so a
 * sentence that does not fit comes back with its last word cut in half. The
 * opening adjective each of these used to carry ("Lab-grade.", "Excellent.",
 * "Weak.") is dropped because the grade letter and the one-word verdict are
 * already on screen beside it saying exactly that. */
static const char* const BLURB[FdyRatingCount] = {
    "Nothing gets through.",
    "Trust it in the field.",
    "Fine for everyday carry.",
    "Leaks a little.",
    "A determined reader wins.",
    "Not shielding at all.",
};
static const uint8_t PIPS[FdyRatingCount] = {5, 4, 3, 2, 1, 0};

const char* fdy_rating_letter(FdyRating r) {
    if(r >= FdyRatingCount) r = FdyRatingF;
    return LETTER[r];
}

const char* fdy_rating_word(FdyRating r) {
    if(r >= FdyRatingCount) r = FdyRatingF;
    return WORD[r];
}

const char* fdy_rating_blurb(FdyRating r) {
    if(r >= FdyRatingCount) r = FdyRatingF;
    return BLURB[r];
}

uint8_t fdy_rating_pips(FdyRating r) {
    if(r >= FdyRatingCount) r = FdyRatingF;
    return PIPS[r];
}

/* Decibel scale for Sub-GHz. A consumer signal-blocking pouch that actually
 * works knocks a fob's carrier down by many tens of dB; a fashion "RFID
 * sleeve" often barely touches it. Thresholds chosen so the grade tracks how a
 * pouch would fare against a real relay / capture attempt, not a lab spec. */
FdyRating fdy_grade_db(int16_t atten_db) {
    if(atten_db >= FDY_DB_APLUS) return FdyRatingAPlus;
    if(atten_db >= FDY_DB_A) return FdyRatingA;
    if(atten_db >= FDY_DB_B) return FdyRatingB;
    if(atten_db >= FDY_DB_C) return FdyRatingC;
    if(atten_db >= FDY_DB_D) return FdyRatingD;
    return FdyRatingF;
}

/* Percentage-of-field scale for NFC. Because a passive card needs the reader
 * field to power up at all, even partial attenuation matters here - but only
 * near-total blocking earns the top grades. */
FdyRating fdy_grade_pct(uint8_t blocked_pct) {
    if(blocked_pct > 100) blocked_pct = 100;
    if(blocked_pct >= FDY_PCT_APLUS) return FdyRatingAPlus;
    if(blocked_pct >= FDY_PCT_A) return FdyRatingA;
    if(blocked_pct >= FDY_PCT_B) return FdyRatingB;
    if(blocked_pct >= FDY_PCT_C) return FdyRatingC;
    if(blocked_pct >= FDY_PCT_D) return FdyRatingD;
    return FdyRatingF;
}
