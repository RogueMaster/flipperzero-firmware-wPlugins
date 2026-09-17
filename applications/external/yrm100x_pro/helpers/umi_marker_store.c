#include "umi_marker_store.h"

#include "../app.h"
#include "debug_log.h"
#include <flipper_format/flipper_format.h>
#include <stdio.h>
#include <string.h>

#define UHF_UMI_MARKERS_PATH      APP_DATA_PATH("UmiMarkers.ff")
#define UHF_UMI_MARKERS_TEMP_PATH APP_DATA_PATH("UmiMarkers.tmp")
#define UHF_UMI_MARKERS_FILE_TYPE "YRM100X_PRO UMI Markers"
#define UHF_UMI_MARKERS_VERSION   1U

/* Field-proven compatibility entry retained from the pre-registry builds. */
static const UHFUmiMarkerEntry uhf_umi_marker_e2801105 = {
    .model_id = {0xE2, 0x80, 0x11, 0x05},
    .word = 0U,
    .mask = 0x0100U,
    .pc3000_bits = 0x0000U,
    .pc3400_bits = 0x0100U,
};

static void uhf_umi_marker_key(char* key, size_t key_size, const char* prefix, uint8_t index) {
    snprintf(key, key_size, "%s %u", prefix, (unsigned int)index);
}

bool uhf_umi_model_id_valid(const uint8_t model_id[UHF_UMI_MODEL_ID_SIZE]) {
    if(!model_id) return false;

    bool any_nonzero = false;
    bool any_not_ff = false;
    for(size_t i = 0; i < UHF_UMI_MODEL_ID_SIZE; i++) {
        any_nonzero |= model_id[i] != 0x00U;
        any_not_ff |= model_id[i] != 0xFFU;
    }
    return any_nonzero && any_not_ff;
}

static bool uhf_umi_marker_entry_valid(const UHFUmiMarkerEntry* entry) {
    if(!entry || !uhf_umi_model_id_valid(entry->model_id)) return false;
    if(entry->word > UHF_UMI_MARKER_MAX_USER_WORD || entry->mask == 0U) return false;
    if((entry->pc3000_bits & (uint16_t)~entry->mask) != 0U) return false;
    if((entry->pc3400_bits & (uint16_t)~entry->mask) != 0U) return false;
    return entry->pc3000_bits != entry->pc3400_bits;
}

void uhf_umi_marker_registry_init(UHFUmiMarkerRegistry* registry) {
    furi_assert(registry);
    memset(registry, 0, sizeof(*registry));
    registry->entries[0] = uhf_umi_marker_e2801105;
    registry->count = 1U;
}

const UHFUmiMarkerEntry* uhf_umi_marker_registry_find(
    const UHFUmiMarkerRegistry* registry,
    const uint8_t model_id[UHF_UMI_MODEL_ID_SIZE]) {
    if(!registry || !uhf_umi_model_id_valid(model_id)) return NULL;

    uint8_t count = registry->count;
    if(count > UHF_UMI_MARKER_MAX_ENTRIES) count = UHF_UMI_MARKER_MAX_ENTRIES;
    for(uint8_t i = 0U; i < count; i++) {
        if(memcmp(registry->entries[i].model_id, model_id, UHF_UMI_MODEL_ID_SIZE) == 0) {
            return &registry->entries[i];
        }
    }
    return NULL;
}

bool uhf_umi_marker_registry_upsert(UHFUmiMarkerRegistry* registry, const UHFUmiMarkerEntry* entry) {
    if(!registry || !uhf_umi_marker_entry_valid(entry)) return false;

    uint8_t count = registry->count;
    if(count > UHF_UMI_MARKER_MAX_ENTRIES) count = UHF_UMI_MARKER_MAX_ENTRIES;
    for(uint8_t i = 0U; i < count; i++) {
        if(memcmp(registry->entries[i].model_id, entry->model_id, UHF_UMI_MODEL_ID_SIZE) == 0) {
            registry->entries[i] = *entry;
            return true;
        }
    }

    if(count < UHF_UMI_MARKER_MAX_ENTRIES) {
        registry->entries[count] = *entry;
        registry->count = count + 1U;
        return true;
    }

    /* Keep the newest learned models when the small on-device table is full. */
    memmove(
        &registry->entries[0],
        &registry->entries[1],
        sizeof(registry->entries[0]) * (UHF_UMI_MARKER_MAX_ENTRIES - 1U));
    registry->entries[UHF_UMI_MARKER_MAX_ENTRIES - 1U] = *entry;
    registry->count = UHF_UMI_MARKER_MAX_ENTRIES;
    return true;
}

bool uhf_umi_marker_registry_load(Storage* storage, UHFUmiMarkerRegistry* registry) {
    furi_assert(storage);
    furi_assert(registry);

    UHFUmiMarkerRegistry loaded_registry;
    uhf_umi_marker_registry_init(&loaded_registry);

    FlipperFormat* format = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0U;
    uint32_t count = 0U;
    bool success = false;

    do {
        if(!flipper_format_file_open_existing(format, UHF_UMI_MARKERS_PATH)) break;
        if(!flipper_format_read_header(format, file_type, &version)) break;
        if(!furi_string_equal_str(file_type, UHF_UMI_MARKERS_FILE_TYPE) ||
           version != UHF_UMI_MARKERS_VERSION) {
            break;
        }
        if(!flipper_format_read_uint32(format, "Count", &count, 1)) break;
        if(count > UHF_UMI_MARKER_MAX_ENTRIES) break;

        for(uint8_t i = 0U; i < (uint8_t)count; i++) {
            UHFUmiMarkerEntry entry = {0};
            uint8_t word_bytes[2] = {0};
            uint8_t mask_bytes[2] = {0};
            uint8_t pc3000_bytes[2] = {0};
            uint8_t pc3400_bytes[2] = {0};
            char key[24];

            uhf_umi_marker_key(key, sizeof(key), "Model", i);
            if(!flipper_format_read_hex(format, key, entry.model_id, sizeof(entry.model_id)))
                break;
            uhf_umi_marker_key(key, sizeof(key), "Word", i);
            if(!flipper_format_read_hex(format, key, word_bytes, sizeof(word_bytes))) break;
            uhf_umi_marker_key(key, sizeof(key), "Mask", i);
            if(!flipper_format_read_hex(format, key, mask_bytes, sizeof(mask_bytes))) break;
            uhf_umi_marker_key(key, sizeof(key), "PC3000", i);
            if(!flipper_format_read_hex(format, key, pc3000_bytes, sizeof(pc3000_bytes))) break;
            uhf_umi_marker_key(key, sizeof(key), "PC3400", i);
            if(!flipper_format_read_hex(format, key, pc3400_bytes, sizeof(pc3400_bytes))) break;

            entry.word = (uint16_t)(((uint16_t)word_bytes[0] << 8) | word_bytes[1]);
            entry.mask = (uint16_t)(((uint16_t)mask_bytes[0] << 8) | mask_bytes[1]);
            entry.pc3000_bits = (uint16_t)(((uint16_t)pc3000_bytes[0] << 8) | pc3000_bytes[1]);
            entry.pc3400_bits = (uint16_t)(((uint16_t)pc3400_bytes[0] << 8) | pc3400_bytes[1]);
            if(!uhf_umi_marker_registry_upsert(&loaded_registry, &entry)) break;

            if(i + 1U == (uint8_t)count) success = true;
        }

        if(count == 0U) success = true;
    } while(false);

    flipper_format_file_close(format);
    furi_string_free(file_type);
    flipper_format_free(format);

    if(success) {
        *registry = loaded_registry;
    } else {
        /* A truncated/corrupt file must never leave a partially loaded table. */
        uhf_umi_marker_registry_init(registry);
    }
    return success;
}

bool uhf_umi_marker_registry_save(Storage* storage, const UHFUmiMarkerRegistry* registry) {
    furi_assert(storage);
    furi_assert(registry);
    if(registry->count > UHF_UMI_MARKER_MAX_ENTRIES) return false;

    FlipperFormat* format = flipper_format_file_alloc(storage);
    bool success = false;

    storage_common_remove(storage, UHF_UMI_MARKERS_TEMP_PATH);
    do {
        if(!flipper_format_file_open_new(format, UHF_UMI_MARKERS_TEMP_PATH)) break;
        if(!flipper_format_write_header_cstr(
               format, UHF_UMI_MARKERS_FILE_TYPE, UHF_UMI_MARKERS_VERSION)) {
            break;
        }

        uint32_t count = registry->count;
        if(!flipper_format_write_uint32(format, "Count", &count, 1)) break;

        for(uint8_t i = 0U; i < registry->count; i++) {
            const UHFUmiMarkerEntry* entry = &registry->entries[i];
            if(!uhf_umi_marker_entry_valid(entry)) break;

            uint8_t word_bytes[2] = {(uint8_t)(entry->word >> 8), (uint8_t)entry->word};
            uint8_t mask_bytes[2] = {(uint8_t)(entry->mask >> 8), (uint8_t)entry->mask};
            uint8_t pc3000_bytes[2] = {
                (uint8_t)(entry->pc3000_bits >> 8),
                (uint8_t)entry->pc3000_bits,
            };
            uint8_t pc3400_bytes[2] = {
                (uint8_t)(entry->pc3400_bits >> 8),
                (uint8_t)entry->pc3400_bits,
            };
            char key[24];

            uhf_umi_marker_key(key, sizeof(key), "Model", i);
            if(!flipper_format_write_hex(format, key, entry->model_id, sizeof(entry->model_id)))
                break;
            uhf_umi_marker_key(key, sizeof(key), "Word", i);
            if(!flipper_format_write_hex(format, key, word_bytes, sizeof(word_bytes))) break;
            uhf_umi_marker_key(key, sizeof(key), "Mask", i);
            if(!flipper_format_write_hex(format, key, mask_bytes, sizeof(mask_bytes))) break;
            uhf_umi_marker_key(key, sizeof(key), "PC3000", i);
            if(!flipper_format_write_hex(format, key, pc3000_bytes, sizeof(pc3000_bytes))) break;
            uhf_umi_marker_key(key, sizeof(key), "PC3400", i);
            if(!flipper_format_write_hex(format, key, pc3400_bytes, sizeof(pc3400_bytes))) break;

            if(i + 1U == registry->count) success = true;
        }

        if(registry->count == 0U) success = true;
    } while(false);

    flipper_format_file_close(format);
    flipper_format_free(format);

    if(!success) {
        storage_common_remove(storage, UHF_UMI_MARKERS_TEMP_PATH);
        return false;
    }
    if(storage_common_rename(storage, UHF_UMI_MARKERS_TEMP_PATH, UHF_UMI_MARKERS_PATH) != FSE_OK) {
        storage_common_remove(storage, UHF_UMI_MARKERS_TEMP_PATH);
        return false;
    }
    return true;
}
