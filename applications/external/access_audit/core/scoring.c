#include "scoring.h"
#include "rules.h"

static uint8_t severity_points(Severity severity) {
    switch(severity) {
    case SeverityInfo:
        return 5;
    case SeverityLow:
        return 10;
    case SeverityMedium:
        return 20;
    case SeverityHigh:
        return 35;
    default:
        return 0;
    }
}

static Severity max_sev(Severity a, Severity b) {
    return (a > b) ? a : b;
}

AuditScore score_observation(const AccessObservation* obs) {
    AuditScore result = {
        .score = 0,
        .confidence = 90,
        .max_severity = SeverityInfo,
    };

    if(!obs || obs->card_type == CardTypeUnknown) {
        result.score = 0;
        result.confidence = 0;
        return result;
    }

    /* ── High-risk rules ── */
    if(rule_legacy_family(obs)) {
        result.score += severity_points(SeverityHigh);
        result.max_severity = max_sev(result.max_severity, SeverityHigh);
    }

    if(rule_identifier_only_pattern(obs)) {
        result.score += severity_points(SeverityHigh);
        result.max_severity = max_sev(result.max_severity, SeverityHigh);
    }

    /* Default keys — additional finding on top of legacy_family for Classic cards.
     * Card keys have never been changed; sector 0 is trivially readable. */
    if(rule_default_keys(obs)) {
        result.score += 15;
        result.max_severity = max_sev(result.max_severity, SeverityHigh);
    }

    /* ── Medium-risk rules ── */
    if(rule_uid_no_memory(obs)) {
        result.score += severity_points(SeverityMedium);
        result.max_severity = max_sev(result.max_severity, SeverityMedium);
    }

    /* ── Low-risk / confidence rules ── */
    if(rule_incomplete_evidence(obs)) {
        result.score += severity_points(SeverityLow);
        result.confidence = (result.confidence >= 20) ? result.confidence - 20 : 0;
        result.max_severity = max_sev(result.max_severity, SeverityLow);
    }

    if(rule_no_uid(obs)) {
        result.score += severity_points(SeverityLow);
        result.confidence = (result.confidence >= 15) ? result.confidence - 15 : 0;
        result.max_severity = max_sev(result.max_severity, SeverityLow);
    }

    /* ── Mitigating rules — reduce score for partial / breakable crypto ── */

    /* Crypto1 is broken but cracking requires active effort (dictionary /
     * hardnested attack), unlike 125 kHz RFID which is a passive serial replay.
     * Apply a small reduction so Classic scores below EM4100-class cards. */
    if(rule_crypto1_breakable(obs)) {
        result.score = (result.score >= 10) ? result.score - 10 : 0;
    }

    if(rule_modern_crypto(obs)) {
        uint8_t reduction = severity_points(SeverityMedium);
        result.score = (result.score >= reduction) ? result.score - reduction : 0;
        /* Reduce severity: HIGH → MEDIUM when not legacy; MEDIUM → Info when score
           is now zero (e.g. DESFire EV2 with no other risk factors). */
        if(!rule_legacy_family(obs)) {
            if(result.max_severity == SeverityHigh) {
                result.max_severity = SeverityMedium;
            } else if(result.max_severity == SeverityMedium && result.score == 0) {
                result.max_severity = SeverityInfo;
            }
        }
    }

    if(result.score > 100) result.score = 100;
    return result;
}

const char* severity_to_string(Severity severity) {
    switch(severity) {
    case SeverityInfo:
        return "Info";
    case SeverityLow:
        return "Low";
    case SeverityMedium:
        return "Medium";
    case SeverityHigh:
        return "High";
    default:
        return "Unknown";
    }
}

const char* card_type_to_string(CardType type) {
    switch(type) {
    case CardTypeEm4100Like:
        return "EM4100";
    case CardTypeHidProxLike:
        return "HID H10301";
    case CardTypeHidGeneric:
        return "HID Generic";
    case CardTypeIndala:
        return "Indala";
    case CardTypeRfid125:
        return "125kHz RFID";
    case CardTypeMifareClassic:
        return "MIFARE Classic";
    case CardTypeMifareClassic1K:
        return "MIFARE Classic 1K";
    case CardTypeMifareClassic4K:
        return "MIFARE Classic 4K";
    case CardTypeMifareClassicMini:
        return "MIFARE Classic Mini";
    case CardTypeMifareUltralight:
        return "MIFARE Ultralight";
    case CardTypeMifareUltralightC:
        return "MIFARE Ultralight C";
    case CardTypeNtag203:
        return "NTAG203";
    case CardTypeNtag213:
        return "NTAG213";
    case CardTypeNtag215:
        return "NTAG215";
    case CardTypeNtag216:
        return "NTAG216";
    case CardTypeNtagI2C:
        return "NTAG I2C";
    case CardTypeMifareDesfire:
        return "MIFARE DESFire";
    case CardTypeMifareDesfireEV1:
        return "DESFire EV1";
    case CardTypeMifareDesfireEV2:
        return "DESFire EV2";
    case CardTypeMifareDesfireEV3:
        return "DESFire EV3";
    case CardTypeMifareDesfireLight:
        return "DESFire Light";
    case CardTypeMifarePlus:
        return "MIFARE Plus";
    case CardTypeMifarePlusSL1:
        return "MIFARE Plus SL1";
    case CardTypeMifarePlusSL2:
        return "MIFARE Plus SL2";
    case CardTypeMifarePlusSL3:
        return "MIFARE Plus SL3";
    case CardTypeIso14443A:
        return "ISO14443-A";
    case CardTypeIso14443B:
        return "ISO14443-B";
    case CardTypeIso15693:
        return "ISO15693";
    case CardTypeHidIclass:
        return "HID iCLASS";
    case CardTypeHidIclassLegacy:
        return "HID iCLASS (Legacy)";
    case CardTypeHidIclassLegacy2k:
        return "HID iCLASS 2k (Legacy)";
    case CardTypeHidIclassLegacy16k:
        return "HID iCLASS 16k (Legacy)";
    case CardTypeHidIclassLegacy32k:
        return "HID iCLASS 32k (Legacy)";
    case CardTypeFelica:
        return "FeliCa";
    case CardTypeFeliCaLite:
        return "FeliCa Lite";
    case CardTypeSlix:
        return "SLIX";
    case CardTypeSt25tb:
        return "ST25TB";
    default:
        return "Unknown";
    }
}
