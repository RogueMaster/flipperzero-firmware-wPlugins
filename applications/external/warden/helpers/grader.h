#pragma once

#include "card_reader.h"

/* Warden's brain: turn a CardReading into a plain-English security grade.
 *
 * The grade is driven by the card *technology*, because the technology decides
 * the crypto. A Mifare Classic is breakable no matter which keys it carries; a
 * DESFire EV2 is sound no matter what it stores. That is exactly how a red-team
 * assessor eyeballs a badge, distilled into a letter and a sentence. */

#define WARDEN_MAX_FINDINGS 6u

typedef enum {
    FindingCritical, // shown as [x] — a break, not a nitpick
    FindingWarn, // [!] — a real weakness / caveat
    FindingGood, // [+] — a genuine strength
    FindingInfo, // [i] — neutral fact about the card
} FindingSeverity;

typedef struct {
    FindingSeverity sev;
    char text[60];
} Finding;

typedef enum {
    RiskBroken, // trivially cloned / defeated
    RiskWeak, // no real crypto; UID-only or open memory
    RiskCaution, // depends on configuration you can't see from outside
    RiskSecure, // modern, mutually-authenticated crypto
} RiskBand;

typedef struct {
    int score; // 0..100, higher = safer
    char letter[4]; // "A+" .. "F"
    RiskBand band;
    char card_name[40]; // e.g. "Mifare Classic 1K"
    char headline[52]; // one-line verdict for the grade card
    char verdict[320]; // the plain-English paragraph
    Finding findings[WARDEN_MAX_FINDINGS];
    size_t finding_num;
} CardGrade;

void grader_evaluate(const CardReading* reading, CardGrade* out);

const char* grader_band_label(RiskBand band); // "SECURE" / "CAUTION" / "WEAK" / "BROKEN"
const char* grader_severity_glyph(FindingSeverity sev); // "[x]" / "[!]" / "[+]" / "[i]"
