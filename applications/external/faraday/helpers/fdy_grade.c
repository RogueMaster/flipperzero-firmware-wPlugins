#include "fdy_grade.h"

static const char* const LETTER[FdyRatingCount] = {"A+", "A", "B", "C", "D", "F"};
static const char* const WORD[FdyRatingCount] =
    {"SEALED", "STRONG", "GOOD", "FAIR", "WEAK", "OPEN"};
static const char* const BLURB[FdyRatingCount] = {
    "Lab-grade. Nothing gets through.",
    "Excellent. Trust it in the field.",
    "Good. Fine for everyday carry.",
    "Fair. Leaks a little - keep it close.",
    "Weak. A determined reader still wins.",
    "Open. This pouch is not shielding.",
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
    if(atten_db >= 60) return FdyRatingAPlus;
    if(atten_db >= 45) return FdyRatingA;
    if(atten_db >= 30) return FdyRatingB;
    if(atten_db >= 20) return FdyRatingC;
    if(atten_db >= 10) return FdyRatingD;
    return FdyRatingF;
}

/* Percentage-of-field scale for NFC. Because a passive card needs the reader
 * field to power up at all, even partial attenuation matters here - but only
 * near-total blocking earns the top grades. */
FdyRating fdy_grade_pct(uint8_t blocked_pct) {
    if(blocked_pct > 100) blocked_pct = 100;
    if(blocked_pct >= 98) return FdyRatingAPlus;
    if(blocked_pct >= 90) return FdyRatingA;
    if(blocked_pct >= 75) return FdyRatingB;
    if(blocked_pct >= 50) return FdyRatingC;
    if(blocked_pct >= 20) return FdyRatingD;
    return FdyRatingF;
}
