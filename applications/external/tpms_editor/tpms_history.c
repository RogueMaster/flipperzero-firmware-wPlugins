#include "tpms_history.h"
#include "tpms_encoder.h"
#include <flipper_format/flipper_format_i.h>
#include <lib/toolbox/stream/stream.h>
#include <lib/subghz/receiver.h>

#include <furi.h>

#define TPMS_HISTORY_MAX 12
#define TAG "TPMSHistory"

typedef struct {
    FuriString* item_str;
    FuriString* protocol;
    FlipperFormat* flipper_string;
    uint8_t type;
    uint32_t id;
    SubGhzRadioPreset* preset;
    int16_t* raw_capture;
    uint16_t raw_capture_count;
    bool raw_capture_truncated;
    int16_t frame_rssi_dbm;
    bool frame_rssi_valid;
    uint8_t wake_profile;
    bool wake_profile_valid;
} TPMSHistoryItem;

ARRAY_DEF(TPMSHistoryItemArray, TPMSHistoryItem, M_POD_OPLIST)
#define M_OPL_TPMSHistoryItemArray_t() ARRAY_OPLIST(TPMSHistoryItemArray, M_POD_OPLIST)

typedef struct {
    TPMSHistoryItemArray_t data;
} TPMSHistoryStruct;

struct TPMSHistory {
    uint32_t last_update_timestamp;
    uint16_t last_index_write;
    uint32_t last_key_hash;
    uint16_t last_affected_index;
    bool last_affected_valid;
    uint16_t next_overwrite_index;
    FuriString* tmp_string;
    TPMSHistoryStruct* history;
};

static uint32_t tpms_history_key_hash(const char* protocol, uint32_t id) {
    uint32_t hash = UINT32_C(2166136261);
    for(const char* p = protocol; p && *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= UINT32_C(16777619);
    }
    hash ^= id;
    hash *= UINT32_C(16777619);
    return hash;
}

static void tpms_history_item_free(TPMSHistoryItem* item) {
    if(!item) return;
    furi_string_free(item->item_str);
    furi_string_free(item->protocol);
    furi_string_free(item->preset->name);
    free(item->preset);
    flipper_format_free(item->flipper_string);
    free(item->raw_capture);
    item->raw_capture = NULL;
    item->raw_capture_count = 0U;
    item->raw_capture_truncated = false;
    item->frame_rssi_dbm = -127;
    item->frame_rssi_valid = false;
    item->wake_profile = TpmsWakeProfileNone;
    item->wake_profile_valid = false;
    item->type = 0U;
}

TPMSHistory* tpms_history_alloc(void) {
    TPMSHistory* instance = malloc(sizeof(TPMSHistory));
    furi_check(instance);
    memset(instance, 0, sizeof(*instance));
    instance->tmp_string = furi_string_alloc();
    instance->history = malloc(sizeof(TPMSHistoryStruct));
    furi_check(instance->history);
    TPMSHistoryItemArray_init(instance->history->data);
    return instance;
}

void tpms_history_free(TPMSHistory* instance) {
    furi_assert(instance);
    furi_string_free(instance->tmp_string);
    for M_EACH(item, instance->history->data, TPMSHistoryItemArray_t) {
        tpms_history_item_free(item);
    }
    TPMSHistoryItemArray_clear(instance->history->data);
    free(instance->history);
    free(instance);
}

uint32_t tpms_history_get_frequency(TPMSHistory* instance, uint16_t idx) {
    furi_assert(instance);
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    return item->preset->frequency;
}

SubGhzRadioPreset* tpms_history_get_radio_preset(TPMSHistory* instance, uint16_t idx) {
    furi_assert(instance);
    return TPMSHistoryItemArray_get(instance->history->data, idx)->preset;
}

const char* tpms_history_get_preset(TPMSHistory* instance, uint16_t idx) {
    furi_assert(instance);
    return furi_string_get_cstr(TPMSHistoryItemArray_get(instance->history->data, idx)->preset->name);
}

void tpms_history_reset(TPMSHistory* instance) {
    furi_assert(instance);
    furi_string_reset(instance->tmp_string);
    for M_EACH(item, instance->history->data, TPMSHistoryItemArray_t) {
        tpms_history_item_free(item);
    }
    TPMSHistoryItemArray_reset(instance->history->data);
    instance->last_index_write = 0U;
    instance->last_key_hash = 0U;
    instance->last_update_timestamp = 0U;
    instance->last_affected_index = 0U;
    instance->last_affected_valid = false;
    instance->next_overwrite_index = 0U;
}

uint16_t tpms_history_get_item(TPMSHistory* instance) {
    furi_assert(instance);
    return instance->last_index_write;
}

uint8_t tpms_history_get_type_protocol(TPMSHistory* instance, uint16_t idx) {
    furi_assert(instance);
    return TPMSHistoryItemArray_get(instance->history->data, idx)->type;
}

const char* tpms_history_get_protocol_name(TPMSHistory* instance, uint16_t idx) {
    furi_assert(instance);
    return furi_string_get_cstr(TPMSHistoryItemArray_get(instance->history->data, idx)->protocol);
}

FlipperFormat* tpms_history_get_raw_data(TPMSHistory* instance, uint16_t idx) {
    furi_assert(instance);
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    return item->flipper_string;
}

bool tpms_history_get_text_space_left(TPMSHistory* instance, FuriString* output) {
    furi_assert(instance);
    if(output) furi_string_printf(output, "%02u/%02u", instance->last_index_write, TPMS_HISTORY_MAX);
    return instance->last_index_write >= TPMS_HISTORY_MAX;
}

void tpms_history_get_text_item_menu(TPMSHistory* instance, FuriString* output, uint16_t idx) {
    furi_assert(instance);
    furi_assert(output);
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    if(item->frame_rssi_valid) {
        furi_string_printf(
            output, "%s %ddBm", furi_string_get_cstr(item->item_str), (int)item->frame_rssi_dbm);
    } else {
        furi_string_set(output, item->item_str);
    }
}

bool tpms_history_set_frame_rssi(TPMSHistory* instance, uint16_t idx, float rssi) {
    furi_assert(instance);
    if(idx >= instance->last_index_write || rssi <= -127.0f || rssi > 20.0f) return false;
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    item->frame_rssi_dbm = (int16_t)(rssi < 0.0f ? rssi - 0.5f : rssi + 0.5f);
    item->frame_rssi_valid = true;
    return true;
}

bool tpms_history_get_frame_rssi(
    TPMSHistory* instance, uint16_t idx, int16_t* rssi_dbm) {
    furi_assert(instance);
    if(idx >= instance->last_index_write || !rssi_dbm) return false;
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    if(!item->frame_rssi_valid) return false;
    *rssi_dbm = item->frame_rssi_dbm;
    return true;
}

static bool tpms_history_read_key(
    FlipperFormat* format,
    FuriString* protocol,
    uint32_t* id) {
    if(!flipper_format_rewind(format)) return false;
    if(!flipper_format_read_string(format, "Protocol", protocol)) return false;
    if(!flipper_format_rewind(format)) return false;
    return flipper_format_read_uint32(format, "Id", id, 1);
}

TPMSHistoryStateAddKey
tpms_history_add_to_history(TPMSHistory* instance, void* context, SubGhzRadioPreset* preset) {
    furi_assert(instance);
    furi_assert(context);
    furi_assert(preset);

    SubGhzProtocolDecoderBase* decoder_base = context;
    FlipperFormat* serialized = flipper_format_string_alloc();
    FuriString* protocol = furi_string_alloc();
    uint32_t id = 0U;
    TPMSHistoryStateAddKey result = TPMSHistoryStateAddKeyUnknown;
    instance->last_affected_valid = false;

    do {
        if(subghz_protocol_decoder_base_serialize(decoder_base, serialized, preset) !=
           SubGhzProtocolStatusOk) {
            FURI_LOG_E(TAG, "Serialize error");
            break;
        }
        if(!tpms_history_read_key(serialized, protocol, &id)) {
            FURI_LOG_E(TAG, "Missing Protocol/Id");
            break;
        }

        const uint32_t now = furi_get_tick();
        const uint32_t key_hash = tpms_history_key_hash(furi_string_get_cstr(protocol), id);
        if(instance->last_key_hash == key_hash && (now - instance->last_update_timestamp) < 500U) {
            instance->last_update_timestamp = now;
            result = TPMSHistoryStateAddKeyTimeOut;
            break;
        }
        instance->last_key_hash = key_hash;
        instance->last_update_timestamp = now;

        /* A sensor is unique by protocol + ID. Test frames intentionally use
         * the same ID across different protocols and must remain separate. */
        for(size_t i = 0U; i < TPMSHistoryItemArray_size(instance->history->data); i++) {
            TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, i);
            if(item->id != id ||
               strcmp(furi_string_get_cstr(item->protocol), furi_string_get_cstr(protocol)) != 0) {
                continue;
            }

            Stream* stream = flipper_format_get_raw_stream(item->flipper_string);
            stream_clean(stream);
            subghz_protocol_decoder_base_serialize(decoder_base, item->flipper_string, preset);
            item->preset->frequency = preset->frequency;
            furi_string_set(item->preset->name, preset->name);
            item->preset->data = preset->data;
            item->preset->data_size = preset->data_size;
            item->type = decoder_base->protocol->type;
            item->wake_profile = TpmsWakeProfileNone;
            item->wake_profile_valid = false;
            instance->last_affected_index = (uint16_t)i;
            instance->last_affected_valid = true;
            result = TPMSHistoryStateAddKeyUpdateData;
            break;
        }
        if(result == TPMSHistoryStateAddKeyUpdateData) break;

        /* FIX2 rolling history: once all 12 GUI slots are occupied, reuse the
           oldest slot instead of stopping with a full-memory warning. Autosave remains
           independent and continues to store every changed decoded payload. */
        const bool replacing = instance->last_index_write >= TPMS_HISTORY_MAX;
        uint16_t target_index = instance->last_index_write;
        TPMSHistoryItem* item = NULL;
        if(replacing) {
            target_index = instance->next_overwrite_index % TPMS_HISTORY_MAX;
            item = TPMSHistoryItemArray_get(instance->history->data, target_index);
            tpms_history_item_free(item);
            instance->next_overwrite_index =
                (uint16_t)((target_index + 1U) % TPMS_HISTORY_MAX);
        } else {
            item = TPMSHistoryItemArray_push_raw(instance->history->data);
        }
        memset(item, 0, sizeof(*item));
        item->preset = malloc(sizeof(SubGhzRadioPreset));
        furi_check(item->preset);
        item->preset->frequency = preset->frequency;
        item->preset->name = furi_string_alloc();
        furi_string_set(item->preset->name, preset->name);
        item->preset->data = preset->data;
        item->preset->data_size = preset->data_size;
        item->type = decoder_base->protocol->type;
        item->id = id;
        item->protocol = furi_string_alloc_set(protocol);
        item->item_str = furi_string_alloc();
        furi_string_printf(
            item->item_str,
            "%s %lX",
            tpms_protocol_list_name(furi_string_get_cstr(protocol)),
            id);
        item->flipper_string = flipper_format_string_alloc();
        subghz_protocol_decoder_base_serialize(decoder_base, item->flipper_string, preset);

        instance->last_affected_index = target_index;
        instance->last_affected_valid = true;
        if(!replacing) instance->last_index_write++;
        result = replacing ? TPMSHistoryStateAddKeyReplaceData :
                             TPMSHistoryStateAddKeyNewDada;
    } while(false);

    furi_string_free(protocol);
    flipper_format_free(serialized);
    return result;
}


bool tpms_history_get_last_affected_index(TPMSHistory* instance, uint16_t* idx) {
    furi_assert(instance);
    if(!idx || !instance->last_affected_valid) return false;
    *idx = instance->last_affected_index;
    return true;
}

bool tpms_history_set_raw_capture(
    TPMSHistory* instance,
    uint16_t idx,
    const int16_t* samples,
    uint16_t count,
    bool truncated) {
    furi_assert(instance);
    if(idx >= instance->last_index_write || !samples || count == 0U ||
       count > TPMS_CAPTURE_MAX_SAMPLES) {
        return false;
    }

    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    int16_t* replacement = malloc((size_t)count * sizeof(int16_t));
    if(!replacement) return false;
    memcpy(replacement, samples, (size_t)count * sizeof(int16_t));

    free(item->raw_capture);
    item->raw_capture = replacement;
    item->raw_capture_count = count;
    item->raw_capture_truncated = truncated;
    return true;
}

bool tpms_history_copy_raw_capture(
    TPMSHistory* instance,
    uint16_t idx,
    int16_t* output,
    uint16_t capacity,
    uint16_t* count,
    bool* truncated) {
    furi_assert(instance);
    if(count) *count = 0U;
    if(truncated) *truncated = false;
    if(idx >= instance->last_index_write || !output || capacity == 0U) return false;

    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    if(!item->raw_capture || item->raw_capture_count == 0U) return false;

    uint16_t copy_count = item->raw_capture_count;
    if(copy_count > capacity) copy_count = capacity;
    memcpy(output, item->raw_capture, (size_t)copy_count * sizeof(int16_t));
    if(count) *count = copy_count;
    if(truncated) {
        *truncated = item->raw_capture_truncated || item->raw_capture_count > capacity;
    }
    return true;
}


bool tpms_history_set_wake_profile(TPMSHistory* instance, uint16_t idx, uint8_t wake_profile) {
    furi_assert(instance);
    if(idx >= instance->last_index_write || wake_profile == TpmsWakeProfileNone ||
       wake_profile > TpmsWakeProfileAutoFordVDO) {
        return false;
    }
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    item->wake_profile = wake_profile;
    item->wake_profile_valid = true;
    return true;
}

bool tpms_history_get_wake_profile(TPMSHistory* instance, uint16_t idx, uint8_t* wake_profile) {
    furi_assert(instance);
    if(idx >= instance->last_index_write || !wake_profile) return false;
    TPMSHistoryItem* item = TPMSHistoryItemArray_get(instance->history->data, idx);
    if(!item->wake_profile_valid) return false;
    *wake_profile = item->wake_profile;
    return true;
}
