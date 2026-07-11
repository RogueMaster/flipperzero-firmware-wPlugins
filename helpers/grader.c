#include "grader.h"
#include <nfc/nfc_device.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ helpers */

static void finding_add(CardGrade* g, FindingSeverity sev, const char* text) {
    if(g->finding_num >= WARDEN_MAX_FINDINGS) return;
    Finding* f = &g->findings[g->finding_num++];
    f->sev = sev;
    strncpy(f->text, text, sizeof(f->text) - 1);
    f->text[sizeof(f->text) - 1] = '\0';
}

static void grade_letter_from_score(int score, char out[4]) {
    const char* s;
    if(score >= 90)
        s = "A+";
    else if(score >= 80)
        s = "A";
    else if(score >= 65)
        s = "B";
    else if(score >= 50)
        s = "C";
    else if(score >= 35)
        s = "D";
    else
        s = "F";
    strncpy(out, s, 3);
    out[3] = '\0';
}

static RiskBand band_from_score(int score) {
    if(score >= 80) return RiskSecure;
    if(score >= 60) return RiskCaution;
    if(score >= 35) return RiskWeak;
    return RiskBroken;
}

const char* grader_band_label(RiskBand band) {
    switch(band) {
    case RiskSecure:
        return "SECURE";
    case RiskCaution:
        return "CAUTION";
    case RiskWeak:
        return "WEAK";
    case RiskBroken:
    default:
        return "BROKEN";
    }
}

const char* grader_severity_glyph(FindingSeverity sev) {
    switch(sev) {
    case FindingCritical:
        return "[x]";
    case FindingWarn:
        return "[!]";
    case FindingGood:
        return "[+]";
    case FindingInfo:
    default:
        return "[i]";
    }
}

/* Mifare Classic sub-type from SAK (the size an access system sees). */
static const char* mf_classic_name_from_sak(uint8_t sak) {
    switch(sak) {
    case 0x09:
        return "Mifare Classic Mini";
    case 0x08:
        return "Mifare Classic 1K";
    case 0x18:
        return "Mifare Classic 4K";
    default:
        return "Mifare Classic";
    }
}

/* A UID that is the *only* secret is clonable to a "magic" card. Access
 * systems that decide on UID alone inherit that weakness whatever the tech. */
static void note_uid(CardGrade* g, const CardReading* r) {
    if(r->uid_len == 4) {
        finding_add(
            g, FindingWarn, "4-byte UID: copyable to a magic card in one tap");
    } else if(r->uid_len == 7) {
        finding_add(g, FindingInfo, "7-byte UID (harder to spoof, not secret)");
    } else if(r->uid_len > 0) {
        finding_add(g, FindingInfo, "UID present on the card (never a secret)");
    }
}

/* ------------------------------------------------------------- per-family */

static void grade_mf_classic(CardGrade* g, const CardReading* r) {
    strncpy(g->card_name, mf_classic_name_from_sak(r->sak), sizeof(g->card_name) - 1);
    g->score = 18;
    strncpy(g->headline, "Broken crypto - treat as cloneable", sizeof(g->headline) - 1);
    finding_add(g, FindingCritical, "Crypto1 cipher broken since 2008");
    finding_add(g, FindingCritical, "Keys recoverable in seconds (nested/darkside)");
    finding_add(g, FindingWarn, "Often ships on default keys (FFFF... / A0A1..)");
    finding_add(g, FindingWarn, "Full 1:1 clone once keys are known");
    note_uid(g, r);
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "Crypto1 has been broken publicly since 2008. A Flipper or Proxmark "
        "recovers this card's keys in seconds to minutes, then clones it bit "
        "for bit. If it opens a door, assume a motivated attacker can too. "
        "Migrate to DESFire EV2/EV3 or Seos.");
}

static void grade_mf_ultralight(CardGrade* g, const CardReading* r) {
    strncpy(g->card_name, "Mifare Ultralight / NTAG", sizeof(g->card_name) - 1);
    g->score = 32;
    strncpy(g->headline, "No crypto - memory & UID readable", sizeof(g->headline) - 1);
    finding_add(g, FindingCritical, "No encryption by default: memory is open");
    finding_add(g, FindingWarn, "UID clonable; NTAG magic tags are common");
    finding_add(g, FindingWarn, "Any password auth is sent in the clear");
    note_uid(g, r);
    finding_add(g, FindingInfo, "OK for tap-to-link/config, not for access");
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "Ultralight/NTAG has no real cryptography - the pages read straight out "
        "and the UID copies onto a magic tag. Some variants add a password, but "
        "it travels unencrypted and is sniffable. Fine for URLs or provisioning, "
        "unsafe as a credential.");
}

static void grade_mf_desfire(CardGrade* g, const CardReading* r) {
    UNUSED(r);
    strncpy(g->card_name, "Mifare DESFire (EV1/EV2/EV3)", sizeof(g->card_name) - 1);
    g->score = 92;
    strncpy(g->headline, "Modern AES - no practical clone", sizeof(g->headline) - 1);
    finding_add(g, FindingGood, "AES-128 / 3DES with mutual authentication");
    finding_add(g, FindingGood, "Per-card diversified keys, encrypted channel");
    finding_add(g, FindingGood, "No practical clone when keys stay off-card");
    finding_add(g, FindingInfo, "Can present a random UID for privacy");
    finding_add(g, FindingWarn, "Only as strong as its key management");
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "DESFire uses AES with mutual authentication and diversified keys, so "
        "the reader and card prove themselves to each other and nothing "
        "clonable ever leaves the card. There is no practical attack on a "
        "correctly provisioned DESFire. Your risk lives in the backend keys, "
        "not this card.");
}

static void grade_mf_plus(CardGrade* g, const CardReading* r) {
    strncpy(g->card_name, "Mifare Plus", sizeof(g->card_name) - 1);
    g->score = 68;
    strncpy(g->headline, "Strong at SL3 - weak if left at SL1", sizeof(g->headline) - 1);
    finding_add(g, FindingGood, "AES available at security level 3");
    finding_add(g, FindingCritical, "At SL1 it emulates broken Classic");
    finding_add(g, FindingWarn, "Deployed level isn't visible from outside");
    note_uid(g, r);
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "Mifare Plus is only as good as the security level it was commissioned "
        "at. At SL3 it is proper AES and sound; at SL1 it behaves like a broken "
        "Classic. You cannot tell which from a read, so confirm the deployment "
        "is SL3 before you trust it.");
}

static void grade_emv(CardGrade* g, const CardReading* r) {
    UNUSED(r);
    strncpy(g->card_name, "Contactless bank card (EMV)", sizeof(g->card_name) - 1);
    g->score = 88;
    strncpy(g->headline, "EMV payment - clone-resistant", sizeof(g->headline) - 1);
    finding_add(g, FindingGood, "Unique cryptogram every tap - no replay");
    finding_add(g, FindingGood, "Contactless uses a token, not your CVV2");
    finding_add(g, FindingGood, "EMV chip: the global payment standard");
    finding_add(g, FindingWarn, "Card number + expiry can be skimmed");
    finding_add(g, FindingInfo, "Charges still need online issuer approval");
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "A contactless EMV bank card. Every tap signs a one-time cryptogram, so "
        "a captured tap can't be replayed and the card can't be cloned for "
        "contactless use. Your PAN and expiry may be skimmable (privacy, not "
        "fraud) - but not the CVV2, and no charge clears without the issuer's "
        "online OK.");
}

static void grade_iso_dep(CardGrade* g, const CardReading* r, bool type_b) {
    UNUSED(r);
    strncpy(
        g->card_name,
        type_b ? "ISO 14443-B smartcard" : "ISO 14443-4 smartcard",
        sizeof(g->card_name) - 1);
    g->score = 72;
    strncpy(g->headline, "Smartcard - depends on the applet", sizeof(g->headline) - 1);
    finding_add(g, FindingGood, "ISO-DEP: full APDU smartcard, not memory");
    finding_add(g, FindingGood, "Can run banking-grade crypto (JavaCard/OS)");
    finding_add(g, FindingWarn, "Security rides on the applet, unknown here");
    finding_add(g, FindingInfo, "Common for payment, transit, national ID");
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "This is a real smartcard speaking ISO-DEP (APDU), the same family as "
        "EMV payment and e-passports. Those can be excellent, but the strength "
        "is in the on-card applet, which a read can't inspect. Assume good "
        "unless you know the deployment is doing something naive.");
}

static void grade_iso15693(CardGrade* g, const CardReading* r) {
    strncpy(g->card_name, "ISO 15693 vicinity tag", sizeof(g->card_name) - 1);
    g->score = 44;
    strncpy(g->headline, "Depends on system - often UID-only", sizeof(g->headline) - 1);
    finding_add(g, FindingWarn, "Plain ICODE/SLIX memory reads openly");
    finding_add(g, FindingWarn, "Many readers trust the UID alone");
    finding_add(g, FindingCritical, "Legacy HID iCLASS shares one broken key");
    finding_add(g, FindingGood, "iCLASS SE / Seos on this layer are strong");
    note_uid(g, r);
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "13.56 MHz vicinity tag. Plain ICODE/SLIX memory reads out, and many "
        "readers trust the UID alone - easily cloned. Legacy HID iCLASS shares "
        "one leaked key; iCLASS SE/Seos on this interface are strong. Identify "
        "which.");
}

static void grade_felica(CardGrade* g, const CardReading* r) {
    UNUSED(r);
    strncpy(g->card_name, "FeliCa", sizeof(g->card_name) - 1);
    g->score = 82;
    strncpy(g->headline, "Mutual auth - strong when provisioned", sizeof(g->headline) - 1);
    finding_add(g, FindingGood, "Mutual authentication with session keys");
    finding_add(g, FindingGood, "Backs transit & e-money (Suica, Octopus)");
    finding_add(g, FindingWarn, "Strength depends on the service provider");
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "FeliCa authenticates both sides and runs an encrypted session, which is "
        "why it carries transit fares and stored value across Asia. Sound by "
        "design; residual risk sits with the service operator's keys, not the "
        "card in your hand.");
}

static void grade_st25tb(CardGrade* g, const CardReading* r) {
    strncpy(g->card_name, "ST25TB / SRIx memory tag", sizeof(g->card_name) - 1);
    g->score = 30;
    strncpy(g->headline, "UID-keyed memory - cloneable", sizeof(g->headline) - 1);
    finding_add(g, FindingCritical, "No cryptographic authentication");
    finding_add(g, FindingWarn, "Access usually keyed on the UID");
    note_uid(g, r);
    finding_add(g, FindingInfo, "Common in transit/ticketing");
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "ST25TB/SRIx is a plain memory tag with no authentication. Systems built "
        "on it lean on the UID, which copies to a blank in seconds. Treat it as "
        "a barcode, not a key.");
}

/* Bare ISO14443-3A with no recognised child protocol: a serial-number token. */
static void grade_bare_iso3a(CardGrade* g, const CardReading* r) {
    strncpy(g->card_name, "ISO 14443-A (UID only)", sizeof(g->card_name) - 1);
    g->score = 28;
    strncpy(g->headline, "UID-only token - trivially cloned", sizeof(g->headline) - 1);
    finding_add(g, FindingCritical, "Presents a UID and little else");
    finding_add(g, FindingWarn, "UID-keyed access clones in one tap");
    note_uid(g, r);
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "This card answers with a UID and not much more. Any door that decides "
        "on the UID alone is opened by a magic card copied in a single tap. "
        "Only safe if the reader runs real crypto on top - which this one "
        "doesn't appear to.");
}

static void grade_unknown(CardGrade* g, const CardReading* r) {
    const char* name = nfc_device_get_protocol_name(r->top);
    strncpy(g->card_name, name ? name : "Unknown NFC card", sizeof(g->card_name) - 1);
    g->score = 50;
    strncpy(g->headline, "Detected - grade inconclusive", sizeof(g->headline) - 1);
    finding_add(g, FindingInfo, "Technology read, deeper detail needed");
    note_uid(g, r);
    snprintf(
        g->verdict,
        sizeof(g->verdict),
        "Warden identified the air interface but not enough to grade the crypto "
        "with confidence. Dump it with the stock NFC app to dig deeper.");
}

/* ------------------------------------------------------------------- entry */

void grader_evaluate(const CardReading* reading, CardGrade* out) {
    furi_assert(reading);
    furi_assert(out);
    memset(out, 0, sizeof(CardGrade));

    switch(reading->top) {
    case NfcProtocolMfClassic:
        grade_mf_classic(out, reading);
        break;
    case NfcProtocolMfUltralight:
        grade_mf_ultralight(out, reading);
        break;
    case NfcProtocolMfDesfire:
        grade_mf_desfire(out, reading);
        break;
    case NfcProtocolMfPlus:
        grade_mf_plus(out, reading);
        break;
    case NfcProtocolIso14443_4a:
        if(reading->is_emv)
            grade_emv(out, reading);
        else
            grade_iso_dep(out, reading, false);
        break;
    case NfcProtocolIso14443_4b:
    case NfcProtocolIso14443_3b:
        if(reading->is_emv)
            grade_emv(out, reading);
        else
            grade_iso_dep(out, reading, true);
        break;
    case NfcProtocolIso15693_3:
    case NfcProtocolSlix:
        grade_iso15693(out, reading);
        break;
    case NfcProtocolFelica:
        grade_felica(out, reading);
        break;
    case NfcProtocolSt25tb:
        grade_st25tb(out, reading);
        break;
    case NfcProtocolIso14443_3a:
        /* SAK bit 5 = ISO14443-4 support: a smartcard the scanner didn't climb
         * into, not a bare UID token. Grade it as ISO-DEP, not F. */
        if(reading->has_iso3a && (reading->sak & 0x20))
            grade_iso_dep(out, reading, false);
        else
            grade_bare_iso3a(out, reading);
        break;
    default:
        grade_unknown(out, reading);
        break;
    }

    if(out->score < 0) out->score = 0;
    if(out->score > 100) out->score = 100;
    grade_letter_from_score(out->score, out->letter);
    out->band = band_from_score(out->score);
}
