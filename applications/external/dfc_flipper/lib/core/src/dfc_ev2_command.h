#pragma once

#include "dfc_common.h"

enum {
    DfcEv2FileHeaderLength = 7,
    DfcEv2UpdateRecordHeaderLength = DFC_UPDATE_RECORD_HEADER_SIZE - 1,
    DfcEv2ValueHeaderLength = 1,
    DfcEv2SetConfigurationHeaderLength = 1,
    DfcEv2CreateTransactionMacHeaderLength = 5,
    DfcEv2ChangeKeyHeaderLength = DFC_CHANGE_KEY_EV2_HEADER_LENGTH - 1,
    DfcEv2ChangeFileSettingsHeaderLength = 1,
    DfcEv2NoEncryptedHeader = UINT8_MAX,
};

typedef enum {
    DfcEv2CommandFile = 1U << 0,
    DfcEv2CommandFileRead = 1U << 1,
    DfcEv2CommandTargetsFile = 1U << 2,
    DfcEv2CommandSupportsEffectivePlain = 1U << 3,
    DfcEv2CommandReaderBypass = 1U << 4,
    DfcEv2CommandEmulatorBypass = 1U << 5,
} DfcEv2CommandFlags;

// Shared facts about a native command that affect EV2 wrapping. Keeping these
// facts together prevents the reader and card paths from assigning different
// clear-header lengths or secure-messaging exceptions to the same instruction.
typedef struct {
    uint8_t flags;
    uint8_t encrypted_header_length;
} DfcEv2CommandMetadata;

// Returns metadata for every instruction byte. Unknown instructions have all
// flags clear and `encrypted_header_length == DfcEv2NoEncryptedHeader`.
DfcEv2CommandMetadata dfc_ev2_command_metadata(uint8_t command);

static inline bool
    dfc_ev2_command_has(const DfcEv2CommandMetadata* metadata, DfcEv2CommandFlags flag) {
    return (metadata->flags & (uint8_t)flag) != 0;
}
