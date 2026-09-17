#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define UHF_UMI_MODEL_ID_SIZE        4U
#define UHF_UMI_MARKER_MAX_ENTRIES   8U
#define UHF_UMI_MARKER_MAX_USER_WORD 31U

/*
 * A TID model ID is the first four bytes of MB2. For EPCglobal TIDs this
 * contains the allocation class plus manufacturer/model fields without the
 * per-tag serial portion.
 *
 * mask selects the User-word bits that control UMI. pc3000_bits and
 * pc3400_bits contain the learned state of only those masked bits. Keeping
 * the other bits out of the record lets Clone preserve unrelated User data.
 */
typedef struct {
    uint8_t model_id[UHF_UMI_MODEL_ID_SIZE];
    uint16_t word;
    uint16_t mask;
    uint16_t pc3000_bits;
    uint16_t pc3400_bits;
} UHFUmiMarkerEntry;

typedef struct {
    uint8_t count;
    UHFUmiMarkerEntry entries[UHF_UMI_MARKER_MAX_ENTRIES];
} UHFUmiMarkerRegistry;

void uhf_umi_marker_registry_init(UHFUmiMarkerRegistry* registry);
bool uhf_umi_marker_registry_load(Storage* storage, UHFUmiMarkerRegistry* registry);
bool uhf_umi_marker_registry_save(Storage* storage, const UHFUmiMarkerRegistry* registry);

const UHFUmiMarkerEntry* uhf_umi_marker_registry_find(
    const UHFUmiMarkerRegistry* registry,
    const uint8_t model_id[UHF_UMI_MODEL_ID_SIZE]);

bool uhf_umi_marker_registry_upsert(UHFUmiMarkerRegistry* registry, const UHFUmiMarkerEntry* entry);

bool uhf_umi_model_id_valid(const uint8_t model_id[UHF_UMI_MODEL_ID_SIZE]);
