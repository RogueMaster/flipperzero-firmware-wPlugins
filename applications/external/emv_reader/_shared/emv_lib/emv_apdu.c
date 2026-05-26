#include "emv_apdu.h"

#include <stdio.h>
#include <string.h>

const uint8_t EMV_AID_VISA[]       = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
const size_t  EMV_AID_VISA_LEN     = 7;
const uint8_t EMV_AID_VISA_DEBIT[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10};
const size_t  EMV_AID_VISA_DEBIT_LEN = 7;
const uint8_t EMV_AID_MASTERCARD[] = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10};
const size_t  EMV_AID_MASTERCARD_LEN = 7;
const uint8_t EMV_AID_MAESTRO[]    = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60};
const size_t  EMV_AID_MAESTRO_LEN  = 7;
const uint8_t EMV_AID_AMEX[]       = {0xA0, 0x00, 0x00, 0x00, 0x25, 0x01};
const size_t  EMV_AID_AMEX_LEN     = 6;
const uint8_t EMV_AID_DISCOVER[]   = {0xA0, 0x00, 0x00, 0x01, 0x52, 0x30, 0x10};
const size_t  EMV_AID_DISCOVER_LEN = 7;
const uint8_t EMV_AID_JCB[]        = {0xA0, 0x00, 0x00, 0x00, 0x65, 0x10, 0x10};
const size_t  EMV_AID_JCB_LEN      = 7;
const uint8_t EMV_AID_INTERAC[]    = {0xA0, 0x00, 0x00, 0x02, 0x77, 0x10, 0x10};
const size_t  EMV_AID_INTERAC_LEN  = 7;

size_t emv_apdu_build_select_ppse(uint8_t* out, size_t out_size) {
    static const uint8_t ppse[] = {'2', 'P', 'A', 'Y', '.', 'S', 'Y', 'S', '.', 'D', 'D', 'F', '0', '1'};
    if(out_size < 5 + sizeof(ppse) + 1) return 0;
    out[0] = 0x00;
    out[1] = EMV_INS_SELECT;
    out[2] = 0x04;
    out[3] = 0x00;
    out[4] = (uint8_t)sizeof(ppse);
    memcpy(out + 5, ppse, sizeof(ppse));
    out[5 + sizeof(ppse)] = 0x00;
    return 5 + sizeof(ppse) + 1;
}

size_t emv_apdu_build_select_aid(
    const uint8_t* aid,
    size_t aid_len,
    uint8_t* out,
    size_t out_size) {
    if(!aid || aid_len == 0 || aid_len > 16) return 0;
    if(out_size < 5 + aid_len + 1) return 0;
    out[0] = 0x00;
    out[1] = EMV_INS_SELECT;
    out[2] = 0x04;
    out[3] = 0x00;
    out[4] = (uint8_t)aid_len;
    memcpy(out + 5, aid, aid_len);
    out[5 + aid_len] = 0x00;
    return 5 + aid_len + 1;
}

size_t emv_apdu_build_gpo(
    const uint8_t* pdol_data,
    size_t pdol_len,
    uint8_t* out,
    size_t out_size) {
    size_t inner_len = 2 + pdol_len;
    if(out_size < 5 + inner_len + 1) return 0;
    if(pdol_len > 252) return 0;
    out[0] = 0x80;
    out[1] = EMV_INS_GPO;
    out[2] = 0x00;
    out[3] = 0x00;
    out[4] = (uint8_t)inner_len;
    out[5] = 0x83;
    out[6] = (uint8_t)pdol_len;
    if(pdol_len > 0 && pdol_data) {
        memcpy(out + 7, pdol_data, pdol_len);
    }
    out[5 + inner_len] = 0x00;
    return 5 + inner_len + 1;
}

size_t emv_apdu_build_read_record(uint8_t sfi, uint8_t record, uint8_t* out, size_t out_size) {
    if(out_size < 5) return 0;
    out[0] = 0x00;
    out[1] = EMV_INS_READ_RECORD;
    out[2] = record;
    out[3] = (uint8_t)((sfi << 3) | 0x04);
    out[4] = 0x00;
    return 5;
}

const char* emv_sw_describe(uint8_t sw1, uint8_t sw2) {
    if(sw1 == 0x90 && sw2 == 0x00) return "OK";
    if(sw1 == 0x61) return "More data available";
    if(sw1 == 0x6C) return "Wrong Le; retry";
    if(sw1 == 0x62) return "Warning, state unchanged";
    if(sw1 == 0x63) return "Warning, state changed";
    if(sw1 == 0x67 && sw2 == 0x00) return "Wrong length";
    if(sw1 == 0x69 && sw2 == 0x82) return "Security status not satisfied";
    if(sw1 == 0x69 && sw2 == 0x83) return "Auth method blocked";
    if(sw1 == 0x69 && sw2 == 0x85) return "Conditions not satisfied";
    if(sw1 == 0x69 && sw2 == 0x86) return "Command not allowed";
    if(sw1 == 0x6A && sw2 == 0x81) return "Function not supported";
    if(sw1 == 0x6A && sw2 == 0x82) return "File not found";
    if(sw1 == 0x6A && sw2 == 0x83) return "Record not found";
    if(sw1 == 0x6A && sw2 == 0x86) return "Incorrect P1/P2";
    if(sw1 == 0x6A && sw2 == 0x88) return "Reference data not found";
    if(sw1 == 0x6D) return "Instruction not supported";
    if(sw1 == 0x6E) return "Class not supported";
    if(sw1 == 0x6F) return "No precise diagnosis";
    return "Unknown SW";
}

bool emv_decode_pan_bcd(const uint8_t* pan_bcd, size_t len, char* out, size_t out_size) {
    if(!pan_bcd || !out || out_size < 2) return 0;
    size_t pos = 0;
    for(size_t i = 0; i < len; i++) {
        uint8_t hi = pan_bcd[i] >> 4;
        uint8_t lo = pan_bcd[i] & 0x0F;
        if(hi == 0xF) break;
        if(pos + 1 >= out_size) return false;
        out[pos++] = '0' + hi;
        if(lo == 0xF) break;
        if(pos + 1 >= out_size) return false;
        out[pos++] = '0' + lo;
    }
    out[pos] = '\0';
    return pos > 0;
}

bool emv_decode_expiry_bcd(const uint8_t* expiry_bcd, size_t len, char* out, size_t out_size) {
    if(!expiry_bcd || len < 3 || !out || out_size < 8) return false;
    uint8_t yy = expiry_bcd[0];
    uint8_t mm = expiry_bcd[1];
    out[0] = '0' + (mm >> 4);
    out[1] = '0' + (mm & 0x0F);
    out[2] = '/';
    out[3] = '0' + (yy >> 4);
    out[4] = '0' + (yy & 0x0F);
    out[5] = '\0';
    return true;
}

bool emv_decode_track2(const uint8_t* track2, size_t len, char* out, size_t out_size) {
    if(!track2 || !out || out_size < 2) return false;
    size_t pos = 0;
    for(size_t i = 0; i < len; i++) {
        uint8_t hi = track2[i] >> 4;
        uint8_t lo = track2[i] & 0x0F;
        if(pos + 1 >= out_size) {
            out[pos] = '\0';
            return true;
        }
        out[pos++] = (hi == 0xD) ? '=' : (hi == 0xF) ? '?' : ('0' + hi);
        if(hi == 0xF) break;
        if(pos + 1 >= out_size) {
            out[pos] = '\0';
            return true;
        }
        out[pos++] = (lo == 0xD) ? '=' : (lo == 0xF) ? '?' : ('0' + lo);
        if(lo == 0xF) break;
    }
    out[pos] = '\0';
    return pos > 0;
}

const char* emv_aid_label(const uint8_t* aid, size_t aid_len) {
    if(!aid) return "Unknown";
    if(aid_len >= EMV_AID_VISA_LEN && memcmp(aid, EMV_AID_VISA, EMV_AID_VISA_LEN) == 0) return "Visa";
    if(aid_len >= EMV_AID_VISA_DEBIT_LEN && memcmp(aid, EMV_AID_VISA_DEBIT, EMV_AID_VISA_DEBIT_LEN) == 0) return "Visa Debit";
    if(aid_len >= EMV_AID_MASTERCARD_LEN && memcmp(aid, EMV_AID_MASTERCARD, EMV_AID_MASTERCARD_LEN) == 0) return "Mastercard";
    if(aid_len >= EMV_AID_MAESTRO_LEN && memcmp(aid, EMV_AID_MAESTRO, EMV_AID_MAESTRO_LEN) == 0) return "Maestro";
    if(aid_len >= EMV_AID_AMEX_LEN && memcmp(aid, EMV_AID_AMEX, EMV_AID_AMEX_LEN) == 0) return "Amex";
    if(aid_len >= EMV_AID_DISCOVER_LEN && memcmp(aid, EMV_AID_DISCOVER, EMV_AID_DISCOVER_LEN) == 0) return "Discover";
    if(aid_len >= EMV_AID_JCB_LEN && memcmp(aid, EMV_AID_JCB, EMV_AID_JCB_LEN) == 0) return "JCB";
    if(aid_len >= EMV_AID_INTERAC_LEN && memcmp(aid, EMV_AID_INTERAC, EMV_AID_INTERAC_LEN) == 0) return "Interac";
    return "Unknown AID";
}

void emv_decode_aip(const uint8_t aip[2], char* out, size_t out_size) {
    if(!out || out_size == 0) return;
    out[0] = '\0';
    size_t pos = 0;

    struct { uint8_t byte; uint8_t bit; const char* label; } flags[] = {
        {0, 0x40, "SDA"},
        {0, 0x20, "DDA"},
        {0, 0x10, "CVM"},
        {0, 0x08, "TRM"},
        {0, 0x04, "IA"},
        {0, 0x01, "CDA"},
        {1, 0x80, "MAG"},
    };

    for(size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        if(aip[flags[i].byte] & flags[i].bit) {
            int n = snprintf(out + pos, out_size - pos, "%s%s", pos > 0 ? "," : "", flags[i].label);
            if(n < 0 || (size_t)n >= out_size - pos) break;
            pos += n;
        }
    }
    if(pos == 0) snprintf(out, out_size, "(none)");
}

bool emv_extract_service_code(const char* track2, char* out, size_t out_size) {
    if(!track2 || !out || out_size < 4) return false;
    const char* sep = NULL;
    for(const char* p = track2; *p; p++) {
        if(*p == '=') { sep = p; break; }
    }
    if(!sep) return false;
    if(!sep[1] || !sep[2] || !sep[3] || !sep[4] || !sep[5] || !sep[6] || !sep[7]) return false;
    out[0] = sep[5];
    out[1] = sep[6];
    out[2] = sep[7];
    out[3] = '\0';
    return true;
}

const char* emv_service_code_describe(const char* sc, char* buf, size_t buf_size) {
    if(!sc || !buf || buf_size == 0 || !sc[0] || !sc[1] || !sc[2]) return "";
    static const char* d1_lut[10] = {
        NULL, "Intl", "IntlIC", NULL, NULL, "Natl", "NatlIC", "Priv", NULL, "Test"
    };
    static const char* d2_lut[10] = {
        "Norm", NULL, "Issuer", NULL, "Issuer*", NULL, NULL, NULL, NULL, NULL
    };
    static const char* d3_lut[10] = {
        "PIN", "Free", "Goods", "ATM", "Cash", "GoodsPIN", "PINpad", "GoodsPINpad", NULL, NULL
    };
    int i1 = sc[0] - '0';
    int i2 = sc[1] - '0';
    int i3 = sc[2] - '0';
    const char* s1 = (i1 >= 0 && i1 < 10 && d1_lut[i1]) ? d1_lut[i1] : "?";
    const char* s2 = (i2 >= 0 && i2 < 10 && d2_lut[i2]) ? d2_lut[i2] : "?";
    const char* s3 = (i3 >= 0 && i3 < 10 && d3_lut[i3]) ? d3_lut[i3] : "?";
    snprintf(buf, buf_size, "%s/%s/%s", s1, s2, s3);
    return buf;
}

bool emv_parse_cvm_list(
    const uint8_t* cvm_buf,
    size_t cvm_len,
    EmvCvmRule* out_rules,
    size_t max_rules,
    size_t* out_count,
    uint32_t* out_amount_x,
    uint32_t* out_amount_y) {
    if(!cvm_buf || cvm_len < 8 || !out_rules || !out_count) return false;

    uint32_t ax = 0, ay = 0;
    for(int i = 0; i < 4; i++) ax = (ax << 8) | cvm_buf[i];
    for(int i = 0; i < 4; i++) ay = (ay << 8) | cvm_buf[4 + i];
    if(out_amount_x) *out_amount_x = ax;
    if(out_amount_y) *out_amount_y = ay;

    size_t count = 0;
    for(size_t pos = 8; pos + 1 < cvm_len && count < max_rules; pos += 2) {
        uint8_t cv = cvm_buf[pos];
        uint8_t cd = cvm_buf[pos + 1];
        out_rules[count].method = cv & 0x3F;
        out_rules[count].fail_continues = (cv & 0x40) != 0;
        out_rules[count].condition = cd;
        out_rules[count].amount_x = ax;
        out_rules[count].amount_y = ay;
        count++;
    }
    *out_count = count;
    return count > 0;
}

const char* emv_cvm_method_label(uint8_t method) {
    switch(method) {
    case EmvCvmFailCardholder:        return "Fail";
    case EmvCvmPlaintextPinIcc:       return "PIN/offline";
    case EmvCvmPlaintextPinOnline:    return "PIN/online";
    case EmvCvmPlaintextPinIccSig:    return "PIN/off+sig";
    case EmvCvmEncipheredPinIcc:      return "PIN/off-enc";
    case EmvCvmEncipheredPinIccSig:   return "PIN/off-enc+sig";
    case EmvCvmSignature:             return "Signature";
    case EmvCvmNoCvm:                 return "NoCVM";
    default:                          return "Unknown";
    }
}

static bool cvm_is_pin_method(uint8_t method) {
    return method == EmvCvmPlaintextPinIcc ||
           method == EmvCvmPlaintextPinOnline ||
           method == EmvCvmPlaintextPinIccSig ||
           method == EmvCvmEncipheredPinIcc ||
           method == EmvCvmEncipheredPinIccSig;
}

EmvPinAnalysis emv_analyze_pin(
    bool aip_present,
    const uint8_t aip[2],
    const EmvCvmRule* rules,
    size_t rules_count) {
    EmvPinAnalysis r = {.status = EmvPinUnknown, .threshold = 0};

    if(!aip_present || (aip[0] & 0x10) == 0) {
        r.status = EmvPinDeferredOnline;
        return r;
    }

    if(!rules || rules_count == 0) {
        r.status = EmvPinUnknown;
        return r;
    }

    bool any_pin_rule = false;
    bool any_always_pin = false;
    uint32_t lowest_above = 0xFFFFFFFFU;
    uint32_t highest_below = 0;
    bool has_above = false;
    bool has_below = false;

    for(size_t i = 0; i < rules_count; i++) {
        if(!cvm_is_pin_method(rules[i].method)) continue;
        any_pin_rule = true;
        switch(rules[i].condition) {
        case EmvCvmCondAlways:
            any_always_pin = true;
            break;
        case EmvCvmCondIfAboveX:
            has_above = true;
            if(rules[i].amount_x < lowest_above) lowest_above = rules[i].amount_x;
            break;
        case EmvCvmCondIfAboveY:
            has_above = true;
            if(rules[i].amount_y < lowest_above) lowest_above = rules[i].amount_y;
            break;
        case EmvCvmCondIfBelowX:
            has_below = true;
            if(rules[i].amount_x > highest_below) highest_below = rules[i].amount_x;
            break;
        case EmvCvmCondIfBelowY:
            has_below = true;
            if(rules[i].amount_y > highest_below) highest_below = rules[i].amount_y;
            break;
        case EmvCvmCondUnattendedCash:
        case EmvCvmCondManualCash:
            any_always_pin = true;
            break;
        default:
            break;
        }
    }

    if(any_always_pin) {
        r.status = EmvPinAlways;
    } else if(has_above) {
        r.status = EmvPinAbove;
        r.threshold = lowest_above;
    } else if(has_below) {
        r.status = EmvPinBelow;
        r.threshold = highest_below;
    } else if(any_pin_rule) {
        r.status = EmvPinUnknown;
    } else {
        r.status = EmvPinNever;
    }
    return r;
}

void emv_format_pin_status(const EmvPinAnalysis* a, char* out, size_t out_size) {
    if(!a || !out || out_size == 0) return;
    switch(a->status) {
    case EmvPinAlways:
        snprintf(out, out_size, "always req by card");
        break;
    case EmvPinAbove:
        snprintf(out, out_size, "card req >$%lu.%02lu",
            (unsigned long)(a->threshold / 100),
            (unsigned long)(a->threshold % 100));
        break;
    case EmvPinBelow:
        snprintf(out, out_size, "card req <$%lu.%02lu",
            (unsigned long)(a->threshold / 100),
            (unsigned long)(a->threshold % 100));
        break;
    case EmvPinNever:
        snprintf(out, out_size, "card never asks");
        break;
    case EmvPinDeferredOnline:
        snprintf(out, out_size, "card never asks");
        break;
    case EmvPinUnknown:
    default:
        snprintf(out, out_size, "?");
        break;
    }
}

const char* emv_cvm_condition_label(uint8_t cond) {
    switch(cond) {
    case EmvCvmCondAlways:             return "always";
    case EmvCvmCondUnattendedCash:     return "unatt-cash";
    case EmvCvmCondNotCashOrManual:    return "non-cash";
    case EmvCvmCondManualCash:         return "manual-cash";
    case EmvCvmCondPurchaseWithCashback: return "purch+cashback";
    case EmvCvmCondIfBelowX:           return "if<X";
    case EmvCvmCondIfAboveX:           return "if>X";
    case EmvCvmCondIfBelowY:           return "if<Y";
    case EmvCvmCondIfAboveY:           return "if>Y";
    default:                           return "?";
    }
}
