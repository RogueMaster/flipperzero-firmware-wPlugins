#pragma once

#include "kdbx/kdbx_data.h"

#include <furi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct App App;
typedef struct FlipPassEditorCustomFieldDraft FlipPassEditorCustomFieldDraft;

typedef enum {
    FlipPassOtpKindNone = 0,
    FlipPassOtpKindHmac,
    FlipPassOtpKindTime,
} FlipPassOtpKind;

typedef enum {
    FlipPassOtpSecretEncodingText = 0,
    FlipPassOtpSecretEncodingHex,
    FlipPassOtpSecretEncodingBase32,
    FlipPassOtpSecretEncodingBase64,
} FlipPassOtpSecretEncoding;

typedef enum {
    FlipPassOtpAlgorithmSha1 = 0,
    FlipPassOtpAlgorithmSha256,
    FlipPassOtpAlgorithmSha512,
} FlipPassOtpAlgorithm;

#define FLIPPASS_OTP_DEFAULT_DIGITS    6U
#define FLIPPASS_OTP_MAX_DIGITS        8U
#define FLIPPASS_OTP_DEFAULT_PERIOD    30U
#define FLIPPASS_OTP_DEFAULT_COUNTER   0ULL
#define FLIPPASS_OTP_CODE_MAX_CHARS    8U
#define FLIPPASS_OTP_COUNTER_TEXT_SIZE 24U

typedef struct {
    FlipPassOtpKind kind;
    FlipPassOtpSecretEncoding secret_encoding;
    FlipPassOtpAlgorithm algorithm;
    const char* secret;
    const char* secret_field_name;
    KDBXCustomField* secret_field;
    KDBXCustomField* counter_field;
    uint8_t digits;
    uint32_t period;
    uint64_t counter;
    bool has_secret;
    bool valid;
    bool too_many_secrets;
} FlipPassOtpConfig;

const char* flippass_otp_kind_label(FlipPassOtpKind kind);
const char*
    flippass_otp_secret_field_name(FlipPassOtpKind kind, FlipPassOtpSecretEncoding encoding);
const char* flippass_otp_algorithm_field_value(FlipPassOtpAlgorithm algorithm);
bool flippass_otp_custom_field_is_reserved(const char* key);
bool flippass_otp_entry_has_any_config(const KDBXEntry* entry);
bool flippass_otp_entry_has_kind(const KDBXEntry* entry, FlipPassOtpKind kind);
bool flippass_otp_draft_is_reserved(const FlipPassEditorCustomFieldDraft* draft);
bool flippass_otp_drafts_have_any_config(const FlipPassEditorCustomFieldDraft* drafts);
void flippass_otp_config_init(FlipPassOtpConfig* config, FlipPassOtpKind kind);
bool flippass_otp_load_config(
    App* app,
    KDBXEntry* entry,
    FlipPassOtpKind kind,
    FlipPassOtpConfig* config,
    FuriString* error);
bool flippass_otp_generate_code(
    App* app,
    KDBXEntry* entry,
    FlipPassOtpKind kind,
    bool advance_hotp,
    char out_code[FLIPPASS_OTP_CODE_MAX_CHARS + 1U],
    FuriString* error);
bool flippass_otp_resolve_autotype_sequence(
    App* app,
    KDBXEntry* entry,
    const char* sequence,
    FuriString* out_sequence,
    FuriString* error);
