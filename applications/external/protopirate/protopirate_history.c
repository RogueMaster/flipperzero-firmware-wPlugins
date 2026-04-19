// protopirate_history.c
#include "protopirate_history.h"
#include "helpers/protopirate_storage.h"
#include <lib/subghz/receiver.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "ProtoPirateHistory"

typedef struct {
    FuriString* item_str;
    FuriString* capture_path;
    uint8_t type;
} ProtoPirateHistoryItem;

ARRAY_DEF(ProtoPirateHistoryItemArray, ProtoPirateHistoryItem, M_POD_OPLIST)

struct ProtoPirateHistory {
    ProtoPirateHistoryItemArray_t data;
    uint16_t last_index;
    uint32_t last_update_timestamp;
    uint8_t code_last_hash_data;
    uint32_t next_capture_seq;
    Storage* storage;
    FlipperFormat* loaded_ff;
    int16_t loaded_idx;
};

void protopirate_history_release_scratch(ProtoPirateHistory* instance) {
    furi_check(instance);
    if(instance->loaded_ff) {
        flipper_format_free(instance->loaded_ff);
        instance->loaded_ff = NULL;
    }
    instance->loaded_idx = -1;
}

static void protopirate_history_item_free(ProtoPirateHistoryItem* item, bool delete_file) {
    if(item->item_str) {
        furi_string_free(item->item_str);
        item->item_str = NULL;
    }
    if(item->capture_path) {
        if(delete_file) {
            protopirate_storage_delete_file(furi_string_get_cstr(item->capture_path));
        }
        furi_string_free(item->capture_path);
        item->capture_path = NULL;
    }
}

ProtoPirateHistory* protopirate_history_alloc(void) {
    ProtoPirateHistory* instance = malloc(sizeof(ProtoPirateHistory));
    furi_check(instance);
    ProtoPirateHistoryItemArray_init(instance->data);
    instance->last_index = 0;
    instance->last_update_timestamp = 0;
    instance->code_last_hash_data = 0;
    instance->next_capture_seq = (uint32_t)(furi_get_tick() & 0x0FFFFFFF);
    if(instance->next_capture_seq == 0) {
        instance->next_capture_seq = 1;
    }
    instance->storage = furi_record_open(RECORD_STORAGE);
    instance->loaded_ff = NULL;
    instance->loaded_idx = -1;
    return instance;
}

void protopirate_history_free(ProtoPirateHistory* instance) {
    furi_check(instance);
    protopirate_history_release_scratch(instance);
    for(size_t i = 0; i < ProtoPirateHistoryItemArray_size(instance->data); i++) {
        ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, i);
        protopirate_history_item_free(item, false);
    }
    ProtoPirateHistoryItemArray_clear(instance->data);
    protopirate_storage_wipe_history_cache();
    if(instance->storage) {
        furi_record_close(RECORD_STORAGE);
        instance->storage = NULL;
    }
    free(instance);
}

void protopirate_history_reset(ProtoPirateHistory* instance) {
    furi_check(instance);
    protopirate_history_release_scratch(instance);
    for(size_t i = 0; i < ProtoPirateHistoryItemArray_size(instance->data); i++) {
        ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, i);
        protopirate_history_item_free(item, false);
    }
    ProtoPirateHistoryItemArray_reset(instance->data);
    instance->last_index = 0;
    protopirate_storage_wipe_history_cache();
}

uint16_t protopirate_history_get_item(ProtoPirateHistory* instance) {
    furi_check(instance);
    return ProtoPirateHistoryItemArray_size(instance->data);
}

uint16_t protopirate_history_get_last_index(ProtoPirateHistory* instance) {
    furi_check(instance);
    return instance->last_index;
}

bool protopirate_history_get_capture_path(
    ProtoPirateHistory* instance,
    uint16_t idx,
    FuriString* out_path) {
    furi_check(instance);
    furi_check(out_path);

    if(idx >= ProtoPirateHistoryItemArray_size(instance->data)) {
        return false;
    }
    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);
    if(!item->capture_path || furi_string_size(item->capture_path) == 0) {
        return false;
    }
    furi_string_set(out_path, item->capture_path);
    return true;
}

bool protopirate_history_add_to_history(
    ProtoPirateHistory* instance,
    void* context,
    SubGhzRadioPreset* preset) {
    furi_check(instance);
    furi_check(context);

    SubGhzProtocolDecoderBase* decoder_base = context;

    if((instance->code_last_hash_data ==
        subghz_protocol_decoder_base_get_hash_data(decoder_base)) &&
       ((furi_get_tick() - instance->last_update_timestamp) < 500)) {
        instance->last_update_timestamp = furi_get_tick();
        return false;
    }

    protopirate_history_release_scratch(instance);

    if(ProtoPirateHistoryItemArray_size(instance->data) >= PROTOPIRATE_HISTORY_MAX) {
        ProtoPirateHistoryItem* oldest = ProtoPirateHistoryItemArray_get(instance->data, 0);
        if(oldest) {
            protopirate_history_item_free(oldest, true);
        }
        ProtoPirateHistoryItemArray_pop_at(NULL, instance->data, 0);
        FURI_LOG_D(TAG, "History full, removed oldest entry");
    }

    instance->code_last_hash_data = subghz_protocol_decoder_base_get_hash_data(decoder_base);
    instance->last_update_timestamp = furi_get_tick();

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_push_raw(instance->data);
    item->item_str = furi_string_alloc();
    item->capture_path = furi_string_alloc();
    item->type = 0;

    FuriString* text = furi_string_alloc();
    subghz_protocol_decoder_base_get_string(decoder_base, text);
    furi_string_set(item->item_str, text);
    furi_string_free(text);

    FlipperFormat* temp_ff = flipper_format_string_alloc();
    furi_check(temp_ff);
    SubGhzProtocolStatus ser =
        subghz_protocol_decoder_base_serialize(decoder_base, temp_ff, preset);
    if(ser != SubGhzProtocolStatusOk) {
        FURI_LOG_E(TAG, "Serialize failed");
        flipper_format_free(temp_ff);
        furi_string_free(item->item_str);
        furi_string_free(item->capture_path);
        ProtoPirateHistoryItemArray_pop_at(
            NULL, instance->data, ProtoPirateHistoryItemArray_size(instance->data) - 1);
        return false;
    }

    uint32_t seq = instance->next_capture_seq++;
    if(!protopirate_storage_save_history_capture(temp_ff, seq, item->capture_path)) {
        FURI_LOG_E(TAG, "Failed to save history file");
        flipper_format_free(temp_ff);
        furi_string_free(item->item_str);
        furi_string_free(item->capture_path);
        ProtoPirateHistoryItemArray_pop_at(
            NULL, instance->data, ProtoPirateHistoryItemArray_size(instance->data) - 1);
        return false;
    }
    flipper_format_rewind(temp_ff);
    {
        uint32_t debug_bit_count;
        FuriString* debug_protocol = furi_string_alloc();
        if(flipper_format_read_string(temp_ff, "Protocol", debug_protocol)) {
            FURI_LOG_I(TAG, "History add - Protocol: %s", furi_string_get_cstr(debug_protocol));
        }
        flipper_format_rewind(temp_ff);
        if(flipper_format_read_uint32(temp_ff, "Bit", &debug_bit_count, 1)) {
            FURI_LOG_I(TAG, "History add - Bit count: %lu", (unsigned long)debug_bit_count);
        }
        furi_string_free(debug_protocol);
    }
    flipper_format_free(temp_ff);

    instance->last_index++;

    FURI_LOG_I(
        TAG,
        "Added item %u to history (size: %zu) path %s",
        instance->last_index,
        ProtoPirateHistoryItemArray_size(instance->data),
        furi_string_get_cstr(item->capture_path));

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

    uint16_t display_idx = idx + 1;
    furi_string_printf(output, "%u. %.*s", display_idx, (int)len, str);
}

void protopirate_history_get_text_item_detail(
    ProtoPirateHistory* instance,
    uint16_t idx,
    FuriString* output,
    SubGhzEnvironment* environment) {
    furi_check(instance);
    furi_check(output);
    UNUSED(environment);

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

    if(instance->loaded_idx == (int16_t)idx && instance->loaded_ff) {
        return instance->loaded_ff;
    }

    protopirate_history_release_scratch(instance);

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);
    if(!item->capture_path || furi_string_size(item->capture_path) == 0) {
        return NULL;
    }

    instance->loaded_ff = flipper_format_file_alloc(instance->storage);
    furi_check(instance->loaded_ff);
    if(!flipper_format_file_open_existing(
           instance->loaded_ff, furi_string_get_cstr(item->capture_path))) {
        FURI_LOG_E(
            TAG, "Failed open history capture %s", furi_string_get_cstr(item->capture_path));
        flipper_format_free(instance->loaded_ff);
        instance->loaded_ff = NULL;
        return NULL;
    }
    instance->loaded_idx = (int16_t)idx;
    return instance->loaded_ff;
}

void protopirate_history_commit_loaded(ProtoPirateHistory* instance) {
    furi_check(instance);
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
