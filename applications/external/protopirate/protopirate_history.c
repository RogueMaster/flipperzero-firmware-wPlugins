// protopirate_history.c
#include "protopirate_history.h"
#include <lib/subghz/receiver.h>
#include <flipper_format/flipper_format_i.h>

#define TAG "ProtoPirateHistory"

typedef struct {
    FuriString* item_str;
    FlipperFormat* flipper_format;
    uint8_t type;
    SubGhzRadioPreset* preset;
} ProtoPirateHistoryItem;

ARRAY_DEF(ProtoPirateHistoryItemArray, ProtoPirateHistoryItem, M_POD_OPLIST)

struct ProtoPirateHistory {
    ProtoPirateHistoryItemArray_t data;
    uint16_t last_index;
    uint32_t last_update_timestamp;
    uint8_t code_last_hash_data;
};

ProtoPirateHistory* protopirate_history_alloc(void) {
    ProtoPirateHistory* instance = malloc(sizeof(ProtoPirateHistory));
    furi_check(instance);
    ProtoPirateHistoryItemArray_init(instance->data);
    instance->last_index = 0;
    instance->last_update_timestamp = 0;
    instance->code_last_hash_data = 0;
    return instance;
}

void protopirate_history_free(ProtoPirateHistory* instance) {
    furi_check(instance);
    for(size_t i = 0; i < ProtoPirateHistoryItemArray_size(instance->data); i++) {
        ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, i);
        furi_string_free(item->item_str);
        flipper_format_free(item->flipper_format);
        if(item->preset) {
            if(item->preset->name) {
                furi_string_free(item->preset->name);
            }
            free(item->preset);
        }
    }
    ProtoPirateHistoryItemArray_clear(instance->data);
    free(instance);
}

void protopirate_history_reset(ProtoPirateHistory* instance) {
    furi_check(instance);
    for(size_t i = 0; i < ProtoPirateHistoryItemArray_size(instance->data); i++) {
        ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, i);
        furi_string_free(item->item_str);
        flipper_format_free(item->flipper_format);
        if(item->preset) {
            if(item->preset->name) {
                furi_string_free(item->preset->name);
            }
            free(item->preset);
        }
    }
    ProtoPirateHistoryItemArray_reset(instance->data);
    instance->last_index = 0;
}

uint16_t protopirate_history_get_item(ProtoPirateHistory* instance) {
    furi_check(instance);
    return ProtoPirateHistoryItemArray_size(instance->data);
}

uint16_t protopirate_history_get_last_index(ProtoPirateHistory* instance) {
    furi_check(instance);
    return instance->last_index;
}

// Helper function to free a single history item's resources
static void protopirate_history_item_free(ProtoPirateHistoryItem* item) {
    if(item->item_str) {
        furi_string_free(item->item_str);
        item->item_str = NULL;
    }
    if(item->flipper_format) {
        flipper_format_free(item->flipper_format);
        item->flipper_format = NULL;
    }
    if(item->preset) {
        if(item->preset->name) {
            furi_string_free(item->preset->name);
        }
        free(item->preset);
        item->preset = NULL;
    }
}

bool protopirate_history_add_to_history(
    ProtoPirateHistory* instance,
    void* context,
    SubGhzRadioPreset* preset) {
    furi_check(instance);
    furi_check(context);

    SubGhzProtocolDecoderBase* decoder_base = context;

    // Check for duplicate (same hash within 500ms)
    if((instance->code_last_hash_data ==
        subghz_protocol_decoder_base_get_hash_data(decoder_base)) &&
       ((furi_get_tick() - instance->last_update_timestamp) < 500)) {
        instance->last_update_timestamp = furi_get_tick();
        return false;
    }

    // If history is full, remove the oldest entry
    if(ProtoPirateHistoryItemArray_size(instance->data) >= PROTOPIRATE_HISTORY_MAX) {
        ProtoPirateHistoryItem* oldest = ProtoPirateHistoryItemArray_get(instance->data, 0);
        if(oldest) {
            protopirate_history_item_free(oldest);
        }
        ProtoPirateHistoryItemArray_pop_at(NULL, instance->data, 0);
        FURI_LOG_D(TAG, "History full, removed oldest entry");
    }

    instance->code_last_hash_data = subghz_protocol_decoder_base_get_hash_data(decoder_base);
    instance->last_update_timestamp = furi_get_tick();

    // Create a new history item
    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_push_raw(instance->data);
    item->item_str = furi_string_alloc();
    item->flipper_format = flipper_format_string_alloc();
    item->type = 0;

    // Copy preset
    item->preset = malloc(sizeof(SubGhzRadioPreset));
    item->preset->frequency = preset->frequency;
    item->preset->name = furi_string_alloc();
    if(preset->name) {
        furi_string_set(item->preset->name, preset->name);
    } else {
        furi_string_set(item->preset->name, "UNKNOWN");
    }
    item->preset->data = preset->data;
    item->preset->data_size = preset->data_size;

    // Get string representation
    FuriString* text = furi_string_alloc();
    subghz_protocol_decoder_base_get_string(decoder_base, text);
    furi_string_set(item->item_str, text);

    // Serialize to flipper format
    subghz_protocol_decoder_base_serialize(decoder_base, item->flipper_format, preset);

    // Debug: Log what we're adding to history
    flipper_format_rewind(item->flipper_format);
    uint32_t debug_bit_count;
    FuriString* debug_protocol = furi_string_alloc();
    if(flipper_format_read_string(item->flipper_format, "Protocol", debug_protocol)) {
        FURI_LOG_I(TAG, "History add - Protocol: %s", furi_string_get_cstr(debug_protocol));
    }
    flipper_format_rewind(item->flipper_format);
    if(flipper_format_read_uint32(item->flipper_format, "Bit", &debug_bit_count, 1)) {
        FURI_LOG_I(TAG, "History add - Bit count: %lu", debug_bit_count);
    }
    furi_string_free(debug_protocol);

    furi_string_free(text);

    instance->last_index++;

    FURI_LOG_I(
        TAG,
        "Added item %u to history (size: %zu)",
        instance->last_index,
        ProtoPirateHistoryItemArray_size(instance->data));

    return true;
}

void protopirate_history_get_text_item_menu(
    ProtoPirateHistory* instance,
    FuriString* output,
    uint16_t idx) {
    furi_check(instance);
    furi_check(output);

    if(idx >= ProtoPirateHistoryItemArray_size(instance->data)) {
        furi_string_set(output, "---");
        return;
    }

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);

    // Get just the first line for the menu
    const char* str = furi_string_get_cstr(item->item_str);
    const char* newline = strchr(str, '\r');
    size_t len = 0;
    if(newline) {
        len = newline - str;
    } else {
        newline = strchr(str, '\n');
        if(newline) {
            len = newline - str;
        } else {
            len = furi_string_size(item->item_str);
        }
    }

    // Add index prefix
    uint16_t display_idx = idx + 1;
    furi_string_printf(output, "%u. %.*s", display_idx, (int)len, str);
}

void protopirate_history_get_text_item(
    ProtoPirateHistory* instance,
    FuriString* output,
    uint16_t idx) {
    furi_check(instance);
    furi_check(output);

    if(idx >= ProtoPirateHistoryItemArray_size(instance->data)) {
        furi_string_set(output, "---");
        return;
    }

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);
    furi_string_set(output, item->item_str);
}

SubGhzProtocolDecoderBase*
    protopirate_history_get_decoder_base(ProtoPirateHistory* instance, uint16_t idx) {
    UNUSED(instance);
    UNUSED(idx);
    return NULL;
}

FlipperFormat* protopirate_history_get_raw_data(ProtoPirateHistory* instance, uint16_t idx) {
    furi_check(instance);

    if(idx >= ProtoPirateHistoryItemArray_size(instance->data)) {
        return NULL;
    }

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);
    return item->flipper_format;
}

void protopirate_history_set_item_str(ProtoPirateHistory* instance, uint16_t idx, const char* str) {
    furi_check(instance);
    furi_check(str);

    if(idx >= ProtoPirateHistoryItemArray_size(instance->data)) {
        return;
    }

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);
    furi_string_set(item->item_str, str);
}
