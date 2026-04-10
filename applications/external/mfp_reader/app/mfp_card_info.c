#include "mfp_card_info.h"

#include <string.h>

/* ---- Manufacturer IDs (ISO/IEC 7816-6 / ISO/IEC 14443-3) ---- */

const char* mfp_card_info_manufacturer_name(uint8_t vendor_id) {
    switch(vendor_id) {
    case 0x01: return "Motorola";
    case 0x02: return "STMicroelectronics";
    case 0x03: return "Hitachi, Ltd.";
    case 0x04: return "NXP Semiconductors Germany";
    case 0x05: return "Infineon Technologies AG";
    case 0x06: return "Cylink";
    case 0x07: return "Texas Instruments";
    case 0x08: return "Fujitsu Limited";
    case 0x09: return "Matsushita Electronics";
    case 0x0A: return "NEC";
    case 0x0B: return "Oki Electric Industry";
    case 0x0C: return "Toshiba Corporation";
    case 0x0D: return "Mitsubishi Electric Corp.";
    case 0x0E: return "Samsung Electronics";
    case 0x0F: return "Hynix Semiconductor";
    case 0x10: return "LG-Semiconductors Co. Ltd.";
    case 0x11: return "Emosyn-EM Microelectronics";
    case 0x12: return "INSIDE Technology";
    case 0x13: return "ORGA Kartensysteme GmbH";
    case 0x14: return "SHARP Corporation";
    case 0x15: return "ATMEL France";
    case 0x16: return "EM Microelectronic-Marin SA";
    case 0x17: return "KSW Microtec GmbH";
    case 0x18: return "ZMD AG";
    case 0x19: return "XICOR, Inc.";
    case 0x1A: return "Sony Corporation";
    case 0x1B: return "Malaysia Microelectronic Solutions";
    case 0x1C: return "Emosyn";
    case 0x1D: return "Shanghai Fudan Microelectronics";
    case 0x1E: return "Magellan Technology Pty Limited";
    case 0x1F: return "Melexis NV BO";
    case 0x20: return "Renesas Technology Corp.";
    case 0x21: return "TAGSYS";
    case 0x22: return "Transcore";
    case 0x23: return "Shanghai Belling Corp., Ltd.";
    case 0x24: return "Masktech Germany GmbH";
    case 0x25: return "Innovision Research and Technology Plc";
    case 0x26: return "Hitachi ULSI Systems Co., Ltd.";
    case 0x27: return "Yubico AB";
    case 0x28: return "Ricoh";
    case 0x29: return "ASK";
    case 0x2A: return "Unicore Microsystems";
    case 0x2B: return "Dallas Semiconductor / Maxim";
    case 0x2C: return "Impinj, Inc.";
    case 0x2D: return "RightPlug Alliance";
    case 0x2E: return "Broadcom Corporation";
    case 0x2F: return "MStar Semiconductor";
    case 0x30: return "BeeDar Technology Inc.";
    default:   return "Unknown";
    }
}

/* ---- SAK decoding ---- */

const char* mfp_card_info_sak_type(uint8_t sak) {
    switch(sak) {
    case 0x00: return "MIFARE Ultralight / NTAG";
    case 0x08: return "MIFARE Classic 1K";
    case 0x09: return "MIFARE Mini";
    case 0x10: return "MIFARE Plus 2K SL2";
    case 0x11: return "MIFARE Plus 4K SL2";
    case 0x18: return "MIFARE Classic 4K / Plus 4K SL1";
    case 0x20: return "MFP SL3 / DESFire / SmartMX";
    case 0x28: return "JCOP30";
    case 0x38: return "MIFARE Classic 4K emulated";
    case 0x88: return "Infineon MIFARE Classic 1K";
    case 0x98: return "Gemplus MPCOS";
    default:   return "Unknown type";
    }
}

/* ---- Main formatting ---- */

/* Helper: append one labeled field in "Label:\nValue\n\n" form.
 * Empty value line is also allowed — the label just stands alone. */
static void append_field(FuriString* out, const char* label, const char* value) {
    /* \e# marks the start of a bold header line; it ends at the next
     * newline. No closing sequence — that would print a literal '#'. */
    furi_string_cat_printf(out, "\e#%s\n", label);
    if(value && *value) {
        furi_string_cat_printf(out, "%s\n", value);
    }
}

void mfp_card_info_format(
    FuriString* out,
    const MfpVersion* version,
    uint8_t sak,
    const uint8_t atqa[2],
    const uint8_t* ats_bytes,
    uint8_t ats_len,
    const uint8_t* block0) {

    furi_string_reset(out);

    FuriString* val = furi_string_alloc();

    /* --- Card type (parsed from ATS historical bytes when available) --- */
    {
        const char* type_name = "MIFARE Plus";
        uint8_t generation = 0;
        if(ats_bytes && ats_len >= 3 && ats_bytes[0] == 0xC1) {
            uint8_t card_type_code = (ats_bytes[2] >> 4) & 0x0F;
            if(card_type_code == 0x2) type_name = "MIFARE Plus";
            else if(card_type_code == 0x3) type_name = "MIFARE Ultralight";
            else if(card_type_code == 0x4) type_name = "MIFARE DESFire";
            if(ats_len >= 4) {
                generation = (ats_bytes[3] >> 4) & 0x0F;
            }
        }
        if(generation == 1) {
            furi_string_printf(val, "%s EV1", type_name);
        } else if(generation == 2) {
            furi_string_printf(val, "%s EV2", type_name);
        } else {
            furi_string_set(val, type_name);
        }
        append_field(out, "Type", furi_string_get_cstr(val));
    }

    /* --- Security Level --- */
    furi_string_printf(val, "SL%d", (int)version->sl);
    append_field(out, "Security Level", furi_string_get_cstr(val));

    /* --- Card Size --- */
    if(ats_bytes && ats_len >= 3 && ats_bytes[0] == 0xC1) {
        uint8_t mem_code = ats_bytes[2] & 0x0F;
        if(mem_code == 0x1) furi_string_set(val, "2K (16 sectors)");
        else if(mem_code == 0x2) furi_string_set(val, "4K (32 sectors)");
        else if(mem_code == 0x3) furi_string_set(val, "4K (40 sectors)");
        else furi_string_set(val, "Unknown");
    } else {
        furi_string_set(val, "Unknown");
    }
    append_field(out, "Card Size", furi_string_get_cstr(val));

    /* --- UID --- */
    furi_string_reset(val);
    for(uint8_t i = 0; i < version->uid_len; i++) {
        furi_string_cat_printf(val, "%s%02X", i == 0 ? "" : " ", version->uid[i]);
    }
    append_field(out, "UID", furi_string_get_cstr(val));

    /* --- Manufacturer from UID[0] --- */
    if(version->uid_len > 0) {
        append_field(
            out, "Manufacturer",
            mfp_card_info_manufacturer_name(version->uid[0]));
    }

    /* --- ATQA --- */
    if(atqa) {
        furi_string_printf(val, "%02X %02X", atqa[0], atqa[1]);
        append_field(out, "ATQA", furi_string_get_cstr(val));
    }

    /* --- SAK --- */
    furi_string_printf(val, "%02X (%s)", sak, mfp_card_info_sak_type(sak));
    append_field(out, "SAK", furi_string_get_cstr(val));

    /* --- ATS --- */
    if(ats_bytes && ats_len > 0) {
        furi_string_reset(val);
        for(uint8_t i = 0; i < ats_len && i < 16; i++) {
            furi_string_cat_printf(val, "%s%02X", i == 0 ? "" : " ", ats_bytes[i]);
        }
        append_field(out, "ATS", furi_string_get_cstr(val));
    }

    /* --- Block 0 validation — BCC check --- */
    if(block0 && version->uid_len > 0) {
        if(version->uid_len == 7) {
            bool uid_match = (memcmp(block0, version->uid, 7) == 0);
            append_field(out, "Block 0 UID", uid_match ? "Match" : "Mismatch");
        } else if(version->uid_len == 4) {
            uint8_t bcc = block0[0] ^ block0[1] ^ block0[2] ^ block0[3];
            bool bcc_ok = (bcc == block0[4]);
            bool uid_match = (memcmp(block0, version->uid, 4) == 0);
            furi_string_printf(
                val, "%s / %s",
                uid_match ? "UID OK" : "UID Mismatch",
                bcc_ok ? "BCC OK" : "BCC BAD");
            append_field(out, "Block 0", furi_string_get_cstr(val));
        }
    }

    furi_string_free(val);
}
