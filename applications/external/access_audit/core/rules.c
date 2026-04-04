#include "rules.h"

bool rule_legacy_family(const AccessObservation* obs) {
    if(!obs) return false;
    /* All 125 kHz RFID protocols lack cryptographic protection */
    if(obs->tech == TechTypeRfid125) return true;
    /* MIFARE Classic family — Crypto1 cipher is broken */
    if(obs->card_type == CardTypeMifareClassic ||
       obs->card_type == CardTypeMifareClassic1K ||
       obs->card_type == CardTypeMifareClassic4K ||
       obs->card_type == CardTypeMifareClassicMini)
        return true;
    /* MIFARE Plus SL1 — Classic-compatibility mode, no AES in use */
    if(obs->card_type == CardTypeMifarePlusSL1) return true;
    /* HID iCLASS Legacy — DES/3DES crypto, master key attack is well-documented.
     * CardTypeHidIclass (unconfirmed TI manufacturer code) is excluded here;
     * it scores MODERATE until the iCLASS scan confirms the Legacy type. */
    if(obs->card_type == CardTypeHidIclassLegacy) return true;
    if(obs->card_type == CardTypeHidIclassLegacy2k) return true;
    if(obs->card_type == CardTypeHidIclassLegacy16k) return true;
    if(obs->card_type == CardTypeHidIclassLegacy32k) return true;
    return false;
}

bool rule_identifier_only_pattern(const AccessObservation* obs) {
    if(!obs) return false;
    /* Card presents a stable UID with no evidence of protected application memory.
     * Any card in this state can be replayed by cloning the UID alone. */
    return obs->uid_present && !obs->user_memory_present && obs->metadata_complete;
}

bool rule_uid_no_memory(const AccessObservation* obs) {
    if(!obs) return false;
    /* Only fire when identifier_only_pattern does not already apply */
    if(rule_identifier_only_pattern(obs)) return false;
    return obs->uid_present && !obs->user_memory_present;
}

bool rule_incomplete_evidence(const AccessObservation* obs) {
    if(!obs) return true;
    return !obs->metadata_complete;
}

bool rule_no_uid(const AccessObservation* obs) {
    if(!obs) return true;
    return !obs->uid_present || obs->uid_len == 0;
}

bool rule_default_keys(const AccessObservation* obs) {
    if(!obs) return false;
    return obs->default_keys_readable;
}

bool rule_modern_crypto(const AccessObservation* obs) {
    if(!obs) return false;
    switch(obs->card_type) {
    /* DESFire family — all variants use DES/3DES or AES */
    case CardTypeMifareDesfire:
    case CardTypeMifareDesfireEV1:
    case CardTypeMifareDesfireEV2:
    case CardTypeMifareDesfireEV3:
    case CardTypeMifareDesfireLight:
    /* MIFARE Plus SL2/SL3 — AES crypto active */
    case CardTypeMifarePlus:
    case CardTypeMifarePlusSL2:
    case CardTypeMifarePlusSL3:
    /* FeliCa Standard uses proprietary crypto — Lite does not */
    case CardTypeFelica:
        return true;
    default:
        return false;
    }
}
