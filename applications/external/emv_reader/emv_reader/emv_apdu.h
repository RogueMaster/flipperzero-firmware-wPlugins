#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMV_INS_SELECT         0xA4
#define EMV_INS_READ_RECORD    0xB2
#define EMV_INS_GPO            0xA8
#define EMV_INS_GENERATE_AC    0xAE
#define EMV_INS_GET_DATA       0xCA
#define EMV_INS_INTERNAL_AUTH  0x88

#define EMV_TAG_PAN            0x5A
#define EMV_TAG_TRACK2_EQUIV   0x57
#define EMV_TAG_HOLDER_NAME    0x5F20
#define EMV_TAG_EXPIRY         0x5F24
#define EMV_TAG_EFFECTIVE      0x5F25
#define EMV_TAG_AID            0x4F
#define EMV_TAG_APP_LABEL      0x50
#define EMV_TAG_APP_PREFNAME   0x9F12
#define EMV_TAG_AIP            0x82
#define EMV_TAG_AFL            0x94
#define EMV_TAG_PDOL           0x9F38
#define EMV_TAG_ATC            0x9F36
#define EMV_TAG_FCI_TEMPLATE   0x6F
#define EMV_TAG_FCI_PROP_TPL   0xA5
#define EMV_TAG_FCI_ISSUER     0xBF0C
#define EMV_TAG_DIR_ENTRY      0x61
#define EMV_TAG_RESPONSE_TPL2  0x77
#define EMV_TAG_RESPONSE_TPL1  0x80

#define EMV_PPSE_NAME "2PAY.SYS.DDF01"
#define EMV_PPSE_NAME_LEN 14

extern const uint8_t EMV_AID_VISA[];
extern const size_t  EMV_AID_VISA_LEN;
extern const uint8_t EMV_AID_VISA_DEBIT[];
extern const size_t  EMV_AID_VISA_DEBIT_LEN;
extern const uint8_t EMV_AID_MASTERCARD[];
extern const size_t  EMV_AID_MASTERCARD_LEN;
extern const uint8_t EMV_AID_MAESTRO[];
extern const size_t  EMV_AID_MAESTRO_LEN;
extern const uint8_t EMV_AID_AMEX[];
extern const size_t  EMV_AID_AMEX_LEN;
extern const uint8_t EMV_AID_DISCOVER[];
extern const size_t  EMV_AID_DISCOVER_LEN;
extern const uint8_t EMV_AID_JCB[];
extern const size_t  EMV_AID_JCB_LEN;
extern const uint8_t EMV_AID_INTERAC[];
extern const size_t  EMV_AID_INTERAC_LEN;

size_t emv_apdu_build_select_ppse(uint8_t* out, size_t out_size);
size_t emv_apdu_build_select_aid(
    const uint8_t* aid,
    size_t aid_len,
    uint8_t* out,
    size_t out_size);
size_t emv_apdu_build_gpo(
    const uint8_t* pdol_data,
    size_t pdol_len,
    uint8_t* out,
    size_t out_size);
size_t emv_apdu_build_read_record(uint8_t sfi, uint8_t record, uint8_t* out, size_t out_size);

const char* emv_sw_describe(uint8_t sw1, uint8_t sw2);

bool emv_decode_pan_bcd(const uint8_t* pan_bcd, size_t len, char* out, size_t out_size);
bool emv_decode_expiry_bcd(const uint8_t* expiry_bcd, size_t len, char* out, size_t out_size);
bool emv_decode_track2(const uint8_t* track2, size_t len, char* out, size_t out_size);

const char* emv_aid_label(const uint8_t* aid, size_t aid_len);

void emv_decode_aip(const uint8_t aip[2], char* out, size_t out_size);

bool emv_extract_service_code(const char* track2, char* out, size_t out_size);
const char* emv_service_code_describe(const char* sc, char* buf, size_t buf_size);

typedef enum {
    EmvCvmFailCardholder = 0x00,
    EmvCvmPlaintextPinIcc = 0x01,
    EmvCvmPlaintextPinOnline = 0x02,
    EmvCvmPlaintextPinIccSig = 0x03,
    EmvCvmEncipheredPinIcc = 0x04,
    EmvCvmEncipheredPinIccSig = 0x05,
    EmvCvmSignature = 0x1E,
    EmvCvmNoCvm = 0x1F,
} EmvCvmMethod;

typedef enum {
    EmvCvmCondAlways = 0x00,
    EmvCvmCondUnattendedCash = 0x01,
    EmvCvmCondNotCashOrManual = 0x02,
    EmvCvmCondManualCash = 0x03,
    EmvCvmCondPurchaseWithCashback = 0x04,
    EmvCvmCondIfBelowX = 0x06,
    EmvCvmCondIfAboveX = 0x07,
    EmvCvmCondIfBelowY = 0x08,
    EmvCvmCondIfAboveY = 0x09,
} EmvCvmCondition;

typedef struct {
    uint8_t method;
    bool fail_continues;
    uint8_t condition;
    uint32_t amount_x;
    uint32_t amount_y;
} EmvCvmRule;

bool emv_parse_cvm_list(
    const uint8_t* cvm_buf,
    size_t cvm_len,
    EmvCvmRule* out_rules,
    size_t max_rules,
    size_t* out_count,
    uint32_t* out_amount_x,
    uint32_t* out_amount_y);

const char* emv_cvm_method_label(uint8_t method);
const char* emv_cvm_condition_label(uint8_t cond);

typedef enum {
    EmvPinAlways,
    EmvPinAbove,
    EmvPinBelow,
    EmvPinNever,
    EmvPinDeferredOnline,
    EmvPinUnknown,
} EmvPinStatus;

typedef struct {
    EmvPinStatus status;
    uint32_t threshold;
} EmvPinAnalysis;

EmvPinAnalysis emv_analyze_pin(
    bool aip_present,
    const uint8_t aip[2],
    const EmvCvmRule* rules,
    size_t rules_count);

void emv_format_pin_status(const EmvPinAnalysis* a, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif
