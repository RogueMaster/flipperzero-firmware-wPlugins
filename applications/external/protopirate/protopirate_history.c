// protopirate_history.c
#include "protopirate_history.h"
#include "helpers/protopirate_storage.h"
#include <lib/subghz/receiver.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <furi.h>

#define TAG                                     "ProtoPirateHistory"
#define PROTOPIRATE_HISTORY_GUARD_SEED_COST     768U
#define PROTOPIRATE_HISTORY_GUARD_FREE_RESERVE  1024U
#define PROTOPIRATE_HISTORY_GUARD_MIN_FREE_HEAP 2048U
#define PROTOPIRATE_HISTORY_GUARD_MIN_MAX_BLOCK 768U

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
    size_t learned_signal_cost;
};

static size_t protopirate_history_get_required_free_heap(ProtoPirateHistory* instance) {
    furi_check(instance);

    size_t learned_cost = instance->learned_signal_cost;
    if(learned_cost < PROTOPIRATE_HISTORY_GUARD_SEED_COST) {
        learned_cost = PROTOPIRATE_HISTORY_GUARD_SEED_COST;
    }

    size_t required_free_heap = learned_cost + PROTOPIRATE_HISTORY_GUARD_FREE_RESERVE;
    if(required_free_heap < PROTOPIRATE_HISTORY_GUARD_MIN_FREE_HEAP) {
        required_free_heap = PROTOPIRATE_HISTORY_GUARD_MIN_FREE_HEAP;
    }

    return required_free_heap;
}

static bool protopirate_history_is_low_memory_now(
    ProtoPirateHistory* instance,
    size_t free_heap,
    size_t max_free_block) {
    furi_check(instance);

    return free_heap < protopirate_history_get_required_free_heap(instance) ||
           max_free_block < PROTOPIRATE_HISTORY_GUARD_MIN_MAX_BLOCK;
}

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
    instance->learned_signal_cost = 0;
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

bool protopirate_history_is_low_memory(ProtoPirateHistory* instance) {
    furi_check(instance);

    size_t free_heap = memmgr_get_free_heap();
    size_t max_free_block = memmgr_heap_get_max_free_block();
    return protopirate_history_is_low_memory_now(instance, free_heap, max_free_block);
}

void protopirate_history_format_status_text(
    ProtoPirateHistory* instance,
    char* output,
    size_t output_size) {
    furi_check(instance);
    furi_check(output);

    if(output_size == 0) {
        return;
    }

    if(protopirate_history_is_low_memory(instance)) {
        snprintf(output, output_size, "RAM!");
        return;
    }

    snprintf(
        output,
        output_size,
        "%u/%u",
        protopirate_history_get_item(instance),
        PROTOPIRATE_HISTORY_MAX);
}

void protopirate_history_get_status_text(ProtoPirateHistory* instance, FuriString* output) {
    furi_check(instance);
    furi_check(output);

    char status_text[16];
    protopirate_history_format_status_text(instance, status_text, sizeof(status_text));
    furi_string_set_str(output, status_text);
}

void protopirate_history_note_signal_allocated(
    ProtoPirateHistory* instance,
    size_t free_heap_before,
    size_t max_free_block_before) {
    furi_check(instance);

    size_t free_heap_after = memmgr_get_free_heap();
    size_t max_free_block_after = memmgr_heap_get_max_free_block();
    size_t learned_signal_cost =
        (free_heap_before > free_heap_after) ? (free_heap_before - free_heap_after) : 0;

    if(learned_signal_cost > instance->learned_signal_cost) {
        instance->learned_signal_cost = learned_signal_cost;
        FURI_LOG_I(
            TAG,
            "Raised learned signal cost to %zu bytes (free %zu->%zu, block %zu->%zu)",
            instance->learned_signal_cost,
            free_heap_before,
            free_heap_after,
            max_free_block_before,
            max_free_block_after);
    }
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

    size_t free_heap = memmgr_get_free_heap();
    size_t max_free_block = memmgr_heap_get_max_free_block();
    size_t required_free_heap = protopirate_history_get_required_free_heap(instance);
    if(protopirate_history_is_low_memory_now(instance, free_heap, max_free_block)) {
        FURI_LOG_W(
            TAG,
            "Skipping history add, free=%zu/%zu block=%zu/%u learned=%zu",
            free_heap,
            required_free_heap,
            max_free_block,
            (size_t)PROTOPIRATE_HISTORY_GUARD_MIN_MAX_BLOCK,
            instance->learned_signal_cost);
        return false;
    }

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

void protopirate_history_delete_item(ProtoPirateHistory* instance, uint16_t idx) {
    furi_check(instance);

    size_t item_count = ProtoPirateHistoryItemArray_size(instance->data);
    if(idx >= item_count) {
        return;
    }

    if(instance->loaded_ff) {
        if(instance->loaded_idx == (int16_t)idx) {
            protopirate_history_release_scratch(instance);
        } else if(instance->loaded_idx > (int16_t)idx) {
            instance->loaded_idx--;
        }
    }

    ProtoPirateHistoryItem* item = ProtoPirateHistoryItemArray_get(instance->data, idx);
    protopirate_history_item_free(item, true);
    ProtoPirateHistoryItemArray_pop_at(NULL, instance->data, idx);

    FURI_LOG_I(
        TAG,
        "Deleted history item %u (size: %zu)",
        idx,
        ProtoPirateHistoryItemArray_size(instance->data));
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
