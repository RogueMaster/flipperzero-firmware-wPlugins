#include "subghz_history.h"
#include <lib/subghz/receiver.h>
#include <rpc/rpc.h>
#include <lib/subghz/protocols/came.h>
#include <lib/subghz/protocols/faac_slh.h>
#include <lib/subghz/protocols/keeloq.h>
#include <lib/subghz/protocols/nice_flor_s.h>

#include <furi.h>

#define TAG "SubGhzHistory"

typedef struct {
    FuriString* item_str;
    FlipperFormat* flipper_string;
    uint16_t tx_block;
    uint8_t type;
    SubGhzRadioPreset* preset;
    DateTime datetime;
    uint32_t hash_data;
    const SubGhzProtocol* protocol;
    uint16_t repeats;
    float latitude;
    float longitude;
} SubGhzHistoryItem;

ARRAY_DEF(SubGhzHistoryItemArray, SubGhzHistoryItem, M_POD_OPLIST) //-V658

#define M_OPL_SubGhzHistoryItemArray_t() ARRAY_OPLIST(SubGhzHistoryItemArray, M_POD_OPLIST)

typedef struct {
    SubGhzHistoryItemArray_t data;
} SubGhzHistoryStruct;

static void subghz_history_tx_block_refresh(SubGhzHistory* instance);

struct SubGhzHistory {
    uint32_t last_update_air_time;
    uint16_t last_index_write;
    uint32_t code_last_hash_data;
    bool code_last_hash_data_set;
    bool full;
    size_t menu_growth;
    FuriString* tmp_string;
    SubGhzHistoryStruct* history;
    Rpc* rpc;
};

SubGhzHistory* subghz_history_alloc(void) {
    SubGhzHistory* instance = malloc(sizeof(SubGhzHistory));
    instance->tmp_string = furi_string_alloc();
    instance->history = malloc(sizeof(SubGhzHistoryStruct));
    SubGhzHistoryItemArray_init(instance->history->data);
    instance->rpc = furi_record_open(RECORD_RPC);
    instance->full = false;
    instance->menu_growth = 0;
    return instance;
}

void subghz_history_free(SubGhzHistory* instance) {
    furi_assert(instance);
    furi_string_free(instance->tmp_string);
    for
        M_EACH(item, instance->history->data, SubGhzHistoryItemArray_t) {
            furi_string_free(item->item_str);
            furi_string_free(item->preset->name);
            free(item->preset);
            flipper_format_free(item->flipper_string);
            item->type = 0;
        }
    SubGhzHistoryItemArray_clear(instance->history->data);
    free(instance->history);
    furi_record_close(RECORD_RPC);
    free(instance);
}

uint32_t subghz_history_get_hash_data(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->hash_data;
}

const SubGhzProtocol* subghz_history_get_protocol(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->protocol;
}

uint16_t subghz_history_get_repeats(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->repeats;
}

uint32_t subghz_history_get_frequency(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->preset->frequency;
}

SubGhzRadioPreset* subghz_history_get_radio_preset(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->preset;
}

const char* subghz_history_get_preset(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return furi_string_get_cstr(item->preset->name);
}

float subghz_history_get_latitude(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->latitude;
}

float subghz_history_get_longitude(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->longitude;
}

void subghz_history_reset(SubGhzHistory* instance) {
    furi_assert(instance);
    furi_string_reset(instance->tmp_string);
    for
        M_EACH(item, instance->history->data, SubGhzHistoryItemArray_t) {
            furi_string_free(item->item_str);
            furi_string_free(item->preset->name);
            free(item->preset);
            flipper_format_free(item->flipper_string);
            item->type = 0;
        }
    SubGhzHistoryItemArray_reset(instance->history->data);
    subghz_txrx_tx_block_reserve(SUBGHZ_TX_MIN_FREE_BLOCK_DEFAULT);
    instance->full = false;
    instance->menu_growth = 0;
    instance->last_index_write = 0;
    instance->code_last_hash_data = 0;
    instance->code_last_hash_data_set = false;
    instance->last_update_air_time = 0;
}

void subghz_history_delete_item(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);

    if(idx < SubGhzHistoryItemArray_size(instance->history->data)) {
        SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
        furi_string_free(item->item_str);
        furi_string_free(item->preset->name);
        free(item->preset);
        flipper_format_free(item->flipper_string);
        item->type = 0;
        SubGhzHistoryItemArray_remove_v(instance->history->data, idx, idx + 1);
        instance->last_index_write--;
        instance->full = false;
        subghz_history_tx_block_refresh(instance);
    }
}

uint16_t subghz_history_get_item(SubGhzHistory* instance) {
    furi_assert(instance);
    return instance->last_index_write;
}

uint8_t subghz_history_get_type_protocol(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->type;
}

const char* subghz_history_get_protocol_name(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    if(!item || !item->flipper_string) {
        FURI_LOG_E(TAG, "Missing Item");
        furi_string_reset(instance->tmp_string);
        return furi_string_get_cstr(instance->tmp_string);
    }
    flipper_format_rewind(item->flipper_string);
    if(!flipper_format_read_string(item->flipper_string, "Protocol", instance->tmp_string)) {
        FURI_LOG_E(TAG, "Missing Protocol");
        furi_string_reset(instance->tmp_string);
    }
    return furi_string_get_cstr(instance->tmp_string);
}

DateTime subghz_history_get_datetime(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    if(item) {
        return item->datetime;
    } else {
        return (DateTime){};
    }
}

FlipperFormat* subghz_history_get_raw_data(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    if(item->flipper_string) {
        return item->flipper_string;
    } else {
        return NULL;
    }
}
static void subghz_history_tx_block_refresh(SubGhzHistory* instance) {
    size_t need = 0;
    for(size_t i = 0; i < SubGhzHistoryItemArray_size(instance->history->data); i++) {
        const uint16_t item_need =
            SubGhzHistoryItemArray_get(instance->history->data, i)->tx_block;
        if(item_need > need) need = item_need;
    }
    subghz_txrx_tx_block_reserve((need != 0) ? need : SUBGHZ_TX_MIN_FREE_BLOCK_DEFAULT);
}

void subghz_history_set_menu_growth(SubGhzHistory* instance, size_t bytes) {
    furi_assert(instance);
    instance->menu_growth = bytes;
}

static size_t subghz_history_growth_bytes(SubGhzHistory* instance) {
    const size_t size = SubGhzHistoryItemArray_size(instance->history->data);
    const size_t capacity = SubGhzHistoryItemArray_capacity(instance->history->data);
    return (size >= capacity) ? M_INC_ALLOC_DEFAULT(capacity) * sizeof(SubGhzHistoryItem) : 0;
}

static size_t subghz_history_heap_need(SubGhzHistory* instance, uint8_t type) {
    const size_t reserve = (type == SubGhzProtocolTypeBinRAW) ? SUBGHZ_HISTORY_RESERVE_RAW :
                                                                SUBGHZ_HISTORY_RESERVE_ITEM;
    // m-lib realloc keeps the old array live while allocating the complete new array.
    return reserve + subghz_history_growth_bytes(instance) + instance->menu_growth;
}

static bool subghz_history_has_room_for(SubGhzHistory* instance, uint8_t type, size_t slack) {
    size_t block = (type == SubGhzProtocolTypeBinRAW) ? SUBGHZ_HISTORY_BLOCK_RAW :
                                                        SUBGHZ_HISTORY_BLOCK_ITEM;
    if(subghz_history_growth_bytes(instance) || instance->menu_growth) {
        // Both arrays can grow on the same capture. Their replacement buffers and the
        // new record must fit before either old array is released, even on a fragmented
        // heap; checking only the largest individual allocation would miss that peak.
        block = subghz_history_heap_need(instance, type);
    }
    return (memmgr_get_free_heap() >= subghz_history_heap_need(instance, type) + slack) &&
           (memmgr_heap_get_max_free_block() >= block + slack + SUBGHZ_HISTORY_BLOCK_OVERHEAD);
}

bool subghz_history_is_full(SubGhzHistory* instance, uint8_t type) {
    furi_assert(instance);
    if(instance->last_index_write >= SUBGHZ_HISTORY_CAPACITY) return true;
    instance->full = !subghz_history_has_room_for(
        instance, type, instance->full ? SUBGHZ_HISTORY_FULL_CLEAR_SLACK : 0);
    return instance->full;
}

bool subghz_history_needs_heap(SubGhzHistory* instance, uint8_t type) {
    furi_assert(instance);
    return memmgr_get_free_heap() < subghz_history_heap_need(instance, type);
}

bool subghz_history_get_text_space_left(
    SubGhzHistory* instance,
    FuriString* output,
    bool ignore_full,
    bool show_sats,
    uint8_t sats,
    uint8_t type) {
    furi_assert(instance);
    if(!ignore_full && subghz_history_is_full(instance, type)) {
        if(output != NULL) furi_string_set(output, "Memory is FULL");
        return true;
    }
    if(output != NULL) {
        if(show_sats) {
            furi_string_printf(output, "%d", sats);
        } else {
            furi_string_printf(output, "%02u", instance->last_index_write);
        }
    }
    return false;
}

uint16_t subghz_history_get_last_index(SubGhzHistory* instance) {
    return instance->last_index_write;
}
void subghz_history_get_text_item_menu(SubGhzHistory* instance, FuriString* output, uint16_t idx) {
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    furi_string_set(output, item->item_str);
}

void subghz_history_get_time_item_menu(SubGhzHistory* instance, FuriString* output, uint16_t idx) {
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    DateTime* t = &item->datetime;
    furi_string_printf(output, "%.2d:%.2d:%.2d ", t->hour, t->minute, t->second);
}

/* Several decoders carry more than one remote family, and each names the one it just decoded
 * on its info screen. The list has to agree, or the same remote reads as two different things
 * depending on where you look. Everything here is read from the serialized signal except the
 * SLH maker, which is resolved from the keystore and deliberately not written to the file. */
static void subghz_history_name_variant(
    FuriString* name,
    FlipperFormat* fff,
    SubGhzProtocolDecoderBase* decoder_base,
    FuriString* scratch) {
    const char* protocol = furi_string_get_cstr(name);
    uint32_t bits = 0;

    if(!flipper_format_rewind(fff) || !flipper_format_read_uint32(fff, "Bit", &bits, 1)) {
        bits = 0;
    }

    if(!strcmp(protocol, SUBGHZ_PROTOCOL_KEELOQ_NAME)) {
        if(flipper_format_rewind(fff) && flipper_format_read_string(fff, "Manufacture", scratch)) {
            furi_string_printf(name, "KL %s", furi_string_get_cstr(scratch));
        }
    } else if(!strcmp(protocol, SUBGHZ_PROTOCOL_FAAC_SLH_NAME)) {
        furi_string_set_str(name, subghz_protocol_decoder_faac_slh_get_variant_name(decoder_base));
    } else if(!strcmp(protocol, SUBGHZ_PROTOCOL_CAME_NAME)) {
        furi_string_set_str(name, subghz_protocol_came_get_variant_name((uint16_t)bits));
    } else if(!strcmp(protocol, SUBGHZ_PROTOCOL_NICE_FLOR_S_NAME)) {
        uint8_t ic[2] = {0};
        uint32_t no_ic = 0;
        const bool o_code =
            (flipper_format_rewind(fff) && flipper_format_read_hex(fff, "IC", ic, sizeof(ic))) ||
            (flipper_format_rewind(fff) && flipper_format_read_uint32(fff, "No_IC", &no_ic, 1));
        furi_string_set_str(
            name, subghz_protocol_nice_flor_s_get_variant_name((uint16_t)bits, o_code));
    }
}

bool subghz_history_add_to_history(
    SubGhzHistory* instance,
    void* context,
    SubGhzRadioPreset* preset,
    uint32_t air_time) {
    furi_assert(instance);
    furi_assert(context);

    SubGhzProtocolDecoderBase* decoder_base = context;
    if(subghz_history_is_full(instance, decoder_base->protocol->type)) return false;
    uint32_t code_hash_data = subghz_protocol_decoder_base_get_hash_data_long(decoder_base);
    //the "have we seen anything at all" flag matters: without it a signal whose hash is
    //zero would match the value the filter starts out with and never reach the history
    //
    //the window is measured in air time, not wall time. a transmitter repeats its frame
    //for as long as the button is held, and the decoders only report a frame once they
    //see the gap that ends it - for the last repeat of a burst that gap is the silence
    //afterwards, which is not measured until the receiver picks up an edge again, so the
    //repeat can be reported seconds after it was actually sent. air time counts what was
    //decoded, so it puts the repeat right next to the ones before it
    if(instance->code_last_hash_data_set && (instance->code_last_hash_data == code_hash_data) &&
       ((air_time - instance->last_update_air_time) < 500)) {
        instance->last_update_air_time = air_time;
        return false;
    }

    const size_t tx_block = subghz_txrx_tx_block_need(decoder_base->protocol->name);
    const size_t held = subghz_txrx_tx_block_size();
    // Growing a transmit reservation must not consume the incoming record's budget.
    if((tx_block > held) &&
       (memmgr_get_free_heap() + held >=
        tx_block + subghz_history_heap_need(instance, decoder_base->protocol->type) +
            SUBGHZ_TX_BLOCK_OVERHEAD)) {
        subghz_txrx_tx_block_reserve(tx_block);
    }

    uint16_t repeats = 0;
    SubGhzHistoryItemArray_it_t it;
    SubGhzHistoryItemArray_it_last(it, instance->history->data);
    while(!SubGhzHistoryItemArray_end_p(it)) {
        SubGhzHistoryItem* search = SubGhzHistoryItemArray_ref(it);
        if(search->hash_data == code_hash_data && search->protocol == decoder_base->protocol) {
            repeats = search->repeats + 1;
            break;
        }
        SubGhzHistoryItemArray_previous(it);
    }

    instance->code_last_hash_data = code_hash_data;
    instance->code_last_hash_data_set = true;
    instance->last_update_air_time = air_time;

    SubGhzHistoryItem* item = SubGhzHistoryItemArray_push_raw(instance->history->data);
    item->preset = malloc(sizeof(SubGhzRadioPreset));
    // Per-signal, so a protocol with more than one frame format gets the right icon
    item->type = subghz_protocol_decoder_base_get_type(decoder_base);
    item->tx_block = (uint16_t)tx_block;
    // if(decoder_base->protocol->filter & SubGhzProtocolFilter_Weather) {
    //     // Other code uses protocol type to check if signal is usable
    //     // so we can't change the actual protocol type, we fake it here
    //     item->type = SubGhzProtocolWeatherStation;
    // }
    item->preset->frequency = preset->frequency;
    item->preset->name = furi_string_alloc();
    furi_string_set(item->preset->name, preset->name);
    item->preset->data = preset->data;
    item->preset->data_size = preset->data_size;
    furi_hal_rtc_get_datetime(&item->datetime);
    item->hash_data = code_hash_data;
    item->protocol = decoder_base->protocol;
    item->repeats = repeats;
    item->latitude = preset->latitude;
    item->longitude = preset->longitude;

    item->item_str = furi_string_alloc();
    item->flipper_string = flipper_format_string_alloc();
    subghz_protocol_decoder_base_serialize(decoder_base, item->flipper_string, preset);

    if(decoder_base->protocol && decoder_base->protocol->decoder &&
       decoder_base->protocol->decoder->get_string_brief) {
        decoder_base->protocol->decoder->get_string_brief(decoder_base, item->item_str);
        instance->last_index_write++;
        return true;
    }

    FuriString* text = furi_string_alloc();

    do {
        if(!flipper_format_rewind(item->flipper_string)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }
        if(!flipper_format_read_string(item->flipper_string, "Protocol", instance->tmp_string)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            break;
        }
        if(!flipper_format_rewind(item->flipper_string)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }
        uint8_t key_data[sizeof(uint64_t)] = {0};
        if(!flipper_format_read_hex(item->flipper_string, "Key", key_data, sizeof(uint64_t))) {
            FURI_LOG_D(TAG, "No Key");
        }
        uint64_t data = 0;
        for(uint8_t i = 0; i < sizeof(uint64_t); i++) {
            data = (data << 8) | key_data[i];
        }
        subghz_history_name_variant(
            instance->tmp_string, item->flipper_string, decoder_base, text);
        if(data != 0) {
            if(!(uint32_t)(data >> 32)) {
                furi_string_printf(
                    item->item_str,
                    "%s %lX",
                    furi_string_get_cstr(instance->tmp_string),
                    (uint32_t)(data & 0xFFFFFFFF));
            } else {
                furi_string_printf(
                    item->item_str,
                    "%s %lX%08lX",
                    furi_string_get_cstr(instance->tmp_string),
                    (uint32_t)(data >> 32),
                    (uint32_t)(data & 0xFFFFFFFF));
            }
        } else {
            furi_string_printf(item->item_str, "%s", furi_string_get_cstr(instance->tmp_string));
        }

    } while(false);

    furi_string_free(text);
    instance->last_index_write++;
    return true;
}

void subghz_history_remove_duplicates(SubGhzHistory* instance) {
    furi_assert(instance);

    SubGhzHistoryItemArray_it_t it;
    SubGhzHistoryItemArray_it_last(it, instance->history->data);
    while(!SubGhzHistoryItemArray_end_p(it)) {
        SubGhzHistoryItem* i = SubGhzHistoryItemArray_ref(it);

        SubGhzHistoryItemArray_it_t jt;
        SubGhzHistoryItemArray_it_set(jt, it);
        SubGhzHistoryItemArray_previous(jt);
        while(!SubGhzHistoryItemArray_end_p(jt)) {
            SubGhzHistoryItem* j = SubGhzHistoryItemArray_ref(jt);

            if(j->hash_data == i->hash_data && j->protocol == i->protocol) {
                subghz_history_delete_item(instance, jt->index);
            }
            SubGhzHistoryItemArray_previous(jt);
        }
        SubGhzHistoryItemArray_previous(it);
    }
}

bool subghz_history_full(SubGhzHistory* instance) {
    return subghz_history_is_full(instance, SubGhzProtocolTypeStatic);
}
