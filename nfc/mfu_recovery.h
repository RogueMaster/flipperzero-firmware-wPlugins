#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <nfc/nfc.h>

typedef struct {
    uint8_t uid[7];
    uint8_t atqa[2];
    uint8_t sak1;
    uint8_t sak2;
    uint8_t bcc0_actual;
    uint8_t bcc0_expected;
    uint8_t bcc1_actual;
    uint8_t bcc1_expected;
    bool bcc0_valid;
    bool bcc1_valid;
    bool needs_recovery;
    uint8_t pages[16];
    bool written;
    bool verified;
    char error_message[32];
} MfuRecoveryResult;

bool mtools_mfu_recover_diagnose(Nfc* nfc, MfuRecoveryResult* result, const bool* running);
bool mtools_mfu_recover_repair(Nfc* nfc, MfuRecoveryResult* result, const bool* running);
