#pragma once

// Capacity and capability checks for admitting a credential to an emulator.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_build_config.h"

#define DFC_PROFILE_REASON_MAX 96

typedef struct {
    // Largest values this build can hold. Zero means "use the build's own
    // compile-time limit", which is the common case.
    size_t max_apps;
    size_t max_files;
    size_t max_keys_per_app;
    // User memory in octets. Zero means unlimited.
    uint32_t max_storage;
    // Whether this target implements files at PICC level.
    bool supports_picc_files;
    // Generations this target emulates, as a bitmask of 1 << DfcGeneration.
    uint8_t supported_generations;
} DfcProfile;

// Bit for a generation, for supported_generations.
#define DFC_PROFILE_GEN_BIT(g) ((uint8_t)(1u << (g)))
#define DFC_PROFILE_GEN_EV1    DFC_PROFILE_GEN_BIT(DfcGenerationEv1)
#define DFC_PROFILE_GEN_EV2    DFC_PROFILE_GEN_BIT(DfcGenerationEv2)
#define DFC_PROFILE_GEN_EV3    DFC_PROFILE_GEN_BIT(DfcGenerationEv3)

// Set the profile from the compile-time feature selection.
void dfc_profile_default(DfcProfile* profile);

// Check `credential` against `profile`. Returns DfcDerOk when it can be
// emulated as-is. On refusal, writes a short reason naming the construct or the
// limit into `reason` when that is non-NULL. An unsupported failure names the construct.
DfcDerStatus dfc_profile_check(
    const DfcProfile* profile,
    const DfcCredential* credential,
    char* reason,
    size_t reason_cap);
