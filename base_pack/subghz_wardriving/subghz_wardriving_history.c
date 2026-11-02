#include "subghz_wardriving_history.h"
#include "helpers/subghz_wardriving_txrx.h"
#include <lib/subghz/receiver.h>
#include <toolbox/stream/stream.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>

#include <furi.h>

#define SUBGHZ_HISTORY_MAX 15

// Room a decoded signal needs. The repeater builds an encoder per received signal, so
// the send block is reserved on top. Every allocation the record makes happens at this
// margin, so it has to hold the largest of them
#define SUBGHZ_HISTORY_RESERVE_ITEM 6144

// A BinRAW capture prints up to 2048 bytes as hex text and grows by realloc, so the old
// and the new copy are live at once - about 17k at the peak
#define SUBGHZ_HISTORY_RESERVE_RAW 14336

// Largest single allocation one record makes, which has to come out of one block. For a
// decoded signal that is the string the protocol serializes itself into, a few hundred
// bytes. Asking for more than it needs makes a delete free nothing until several freed
// records happen to merge, which is what made one signal drop a whole run of them. The
// string is grown by doubling, so the last step asks for as much again as it already holds
#define SUBGHZ_HISTORY_BLOCK_ITEM 1024
#define SUBGHZ_HISTORY_BLOCK_RAW 6144

// What the allocator puts in front of a block, rounded up to its alignment. A free block
// of exactly N bytes cannot serve a request for N, so every size checked here carries this
// on top - and no more than this, or a block just freed reads as too small to take back
#define SUBGHZ_HISTORY_BLOCK_OVERHEAD 8

#define TAG "SubGhzWarDrivingHistory"

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
    bool saved;
} SubGhzHistoryItem;

ARRAY_DEF(SubGhzHistoryItemArray, SubGhzHistoryItem, M_POD_OPLIST) //-V658

#define M_OPL_SubGhzHistoryItemArray_t() ARRAY_OPLIST(SubGhzHistoryItemArray, M_POD_OPLIST)

typedef struct {
    SubGhzHistoryItemArray_t data;
} SubGhzHistoryStruct;

static void subghz_wardriving_history_tx_block_refresh(SubGhzHistory* instance);

struct SubGhzHistory {
    uint32_t last_update_timestamp;
    uint16_t last_index_write;
    uint32_t code_last_hash_data;
    bool full;
    FuriString* tmp_string;
    SubGhzHistoryStruct* history;
};

SubGhzHistory* subghz_wardriving_history_alloc(void) {
    SubGhzHistory* instance = malloc(sizeof(SubGhzHistory));
    instance->tmp_string = furi_string_alloc();
    instance->history = malloc(sizeof(SubGhzHistoryStruct));
    SubGhzHistoryItemArray_init(instance->history->data);
    instance->full = false;
    return instance;
}

void subghz_wardriving_history_free(SubGhzHistory* instance) {
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
    free(instance);
}

uint32_t subghz_wardriving_history_get_hash_data(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->hash_data;
}

const SubGhzProtocol*
    subghz_wardriving_history_get_protocol(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->protocol;
}

uint16_t subghz_wardriving_history_get_repeats(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->repeats;
}

uint32_t subghz_wardriving_history_get_frequency(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->preset->frequency;
}

SubGhzRadioPreset*
    subghz_wardriving_history_get_radio_preset(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->preset;
}

const char* subghz_wardriving_history_get_preset(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return furi_string_get_cstr(item->preset->name);
}

float subghz_wardriving_history_get_latitude(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->latitude;
}

float subghz_wardriving_history_get_longitude(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->longitude;
}

void subghz_wardriving_history_reset(SubGhzHistory* instance) {
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
    subghz_wardriving_txrx_tx_block_reserve(SUBGHZ_TX_MIN_FREE_BLOCK_DEFAULT);
    instance->full = false;
    instance->last_index_write = 0;
    instance->code_last_hash_data = 0;
}

void subghz_wardriving_history_delete_item(SubGhzHistory* instance, uint16_t idx) {
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
        // Something real changed, so the next check measures the heap as it is now
        instance->full = false;
        subghz_wardriving_history_tx_block_refresh(instance);
    }
}

uint16_t subghz_wardriving_history_get_item(SubGhzHistory* instance) {
    furi_assert(instance);
    return instance->last_index_write;
}

uint8_t subghz_wardriving_history_get_type_protocol(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    return item->type;
}

const char* subghz_wardriving_history_get_protocol_name(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    if(!item || !item->flipper_string) {
        FURI_LOG_E(TAG, "Missing Item");
        furi_string_reset(instance->tmp_string);
        return furi_string_get_cstr(instance->tmp_string);
    }
    flipper_format_rewind(item->flipper_string);
    if(!flipper_format_read_string(item->flipper_string, "Protocol", instance->tmp_string)) {
        FURI_LOG_E(TAG, "Missing Protocol get_protocol_name");
        furi_string_reset(instance->tmp_string);
    }
    return furi_string_get_cstr(instance->tmp_string);
}

DateTime subghz_wardriving_history_get_datetime(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    if(item) {
        return item->datetime;
    } else {
        return (DateTime){};
    }
}

FlipperFormat* subghz_wardriving_history_get_raw_data(SubGhzHistory* instance, uint16_t idx) {
    furi_assert(instance);
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    if(item->flipper_string) {
        return item->flipper_string;
    } else {
        return NULL;
    }
}

// The held send block only has to cover the widest protocol still in the list, so it is
// resized whenever that changes. Growing can fail, which is what turns a signal away
static void subghz_wardriving_history_tx_block_refresh(SubGhzHistory* instance) {
    size_t need = 0;
    for(size_t i = 0; i < SubGhzHistoryItemArray_size(instance->history->data); i++) {
        const uint16_t item_need =
            SubGhzHistoryItemArray_get(instance->history->data, i)->tx_block;
        if(item_need > need) need = item_need;
    }
    subghz_wardriving_txrx_tx_block_reserve(
        (need != 0) ? need : SUBGHZ_TX_MIN_FREE_BLOCK_DEFAULT);
}

// Is there heap left for one more signal of this type, with a send still possible. The
// slack is what a history that already reads full has to get back before it reads otherwise
static bool subghz_wardriving_history_has_room_for(uint8_t type, size_t slack) {
    const bool raw = (type == SubGhzProtocolTypeBinRAW);
    const size_t reserve =
        (raw ? SUBGHZ_HISTORY_RESERVE_RAW : SUBGHZ_HISTORY_RESERVE_ITEM) + slack;
    size_t block = raw ? SUBGHZ_HISTORY_BLOCK_RAW : SUBGHZ_HISTORY_BLOCK_ITEM;
    // Only what the record itself asks for is measured here. Whether a send is still
    // possible is the send block's business and the popup's: a receiver that stops
    // receiving because a send might not fit is the wrong way round, and the block it
    // would be waiting for is one that dropping records cannot produce
    block += slack + SUBGHZ_HISTORY_BLOCK_OVERHEAD;
    return (memmgr_get_free_heap() >= reserve) && (memmgr_heap_get_max_free_block() >= block);
}

// Is what the history is short of plain heap? Dropping a record always returns some, so
// while this is true dropping another one gets closer to room. A block wide enough for a
// big signal is the other case: that only appears if freed records happen to sit next to
// each other, and a run of small ones never will, so whoever is making room has to stop
// there instead of emptying the list for a signal that gets turned away anyway
bool subghz_wardriving_history_needs_heap(SubGhzHistory* instance, uint8_t type) {
    furi_assert(instance);
    const size_t reserve = (type == SubGhzProtocolTypeBinRAW) ? SUBGHZ_HISTORY_RESERVE_RAW :
                                                                SUBGHZ_HISTORY_RESERVE_ITEM;
    return memmgr_get_free_heap() < reserve;
}

bool subghz_wardriving_history_full(SubGhzHistory* instance, uint8_t type) {
    furi_assert(instance);
    if(instance->last_index_write >= SUBGHZ_HISTORY_MAX) return true;
    instance->full = !subghz_wardriving_history_has_room_for(
        type, instance->full ? SUBGHZ_HISTORY_FULL_CLEAR_SLACK : 0);
    return instance->full;
}

bool subghz_wardriving_history_get_text_space_left(
    SubGhzHistory* instance,
    FuriString* output,
    bool ignore_full,
    bool show_sats,
    uint8_t sats,
    uint8_t type) {
    furi_assert(instance);
    if(!ignore_full) {
        if(instance->last_index_write >= SUBGHZ_HISTORY_MAX) {
            if(output != NULL) furi_string_set(output, "History is FULL");
            return true;
        }
        if(subghz_wardriving_history_full(instance, type)) {
            if(output != NULL) furi_string_set(output, "Memory is FULL");
            return true;
        }
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

uint16_t subghz_wardriving_history_get_last_index(SubGhzHistory* instance) {
    return instance->last_index_write;
}
void subghz_wardriving_history_get_text_item_menu(
    SubGhzHistory* instance,
    FuriString* output,
    uint16_t idx) {
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    furi_string_set(output, item->item_str);
}

void subghz_wardriving_history_get_time_item_menu(
    SubGhzHistory* instance,
    FuriString* output,
    uint16_t idx) {
    SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, idx);
    DateTime* t = &item->datetime;
    furi_string_printf(output, "%.2d:%.2d:%.2d ", t->hour, t->minute, t->second);
}

bool subghz_wardriving_history_add_to_history(
    SubGhzHistory* instance,
    void* context,
    SubGhzRadioPreset* preset,
    float latitude,
    float longitude) {
    furi_assert(instance);
    furi_assert(context);

    SubGhzProtocolDecoderBase* decoder_base = context;
    if(subghz_wardriving_history_full(instance, decoder_base->protocol->type)) return false;

    uint32_t hash_data = subghz_protocol_decoder_base_get_hash_data(decoder_base);
    if((instance->code_last_hash_data == hash_data) &&
       ((furi_get_tick() - instance->last_update_timestamp) < 600)) {
        instance->last_update_timestamp = furi_get_tick();
        return false;
    }

    // A signal is only worth keeping if it can still be sent, so the block grows to it
    // first. Everything narrower fits the block this one needs. A repeat that was just
    // suppressed never gets here, so it cannot grow the block for a record nobody keeps
    // Failing is allowed: the record is kept either way and the send says so if it comes to
    // that, which beats turning away signals over a block no amount of deleting can produce
    const size_t tx_block = subghz_wardriving_txrx_tx_block_need(decoder_base->protocol->name);
    if(tx_block > subghz_wardriving_txrx_tx_block_size()) {
        subghz_wardriving_txrx_tx_block_reserve(tx_block);
    }

    uint16_t repeats = 0;
    SubGhzHistoryItemArray_it_t it;
    SubGhzHistoryItemArray_it_last(it, instance->history->data);
    while(!SubGhzHistoryItemArray_end_p(it)) {
        SubGhzHistoryItem* search = SubGhzHistoryItemArray_ref(it);
        if(search->hash_data == hash_data && search->protocol == decoder_base->protocol) {
            repeats = search->repeats + 1;
            break;
        }
        SubGhzHistoryItemArray_previous(it);
    }

    instance->code_last_hash_data = hash_data;
    instance->last_update_timestamp = furi_get_tick();

    SubGhzHistoryItem* item = SubGhzHistoryItemArray_push_raw(instance->history->data);
    item->preset = malloc(sizeof(SubGhzRadioPreset));
    item->type = decoder_base->protocol->type;
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
    item->hash_data = hash_data;
    item->protocol = decoder_base->protocol;
    item->repeats = repeats;
    item->latitude = latitude;
    item->longitude = longitude;
    item->saved = true;

    item->item_str = furi_string_alloc();
    item->flipper_string = flipper_format_string_alloc();
    subghz_protocol_decoder_base_serialize(decoder_base, item->flipper_string, preset);

    do {
        if(!flipper_format_rewind(item->flipper_string)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }
        if(!flipper_format_read_string(item->flipper_string, "Protocol", instance->tmp_string)) {
            FURI_LOG_E(TAG, "Missing Protocol add_to_history");
            break;
        }

        // Rewind to line before Protocol
        Stream* stream = flipper_format_get_raw_stream(item->flipper_string);
        if(!stream_seek_to_char(stream, '\n', StreamDirectionBackward)) {
            FURI_LOG_E(TAG, "Seek 1 failed");
            break;
        }
        if(!stream_seek(stream, 1, StreamOffsetFromCurrent)) {
            FURI_LOG_E(TAG, "Seek 2 failed");
            break;
        }
        // Insert Lat and Lon at the right place
        if(!stream_insert_format(stream, "Lat: %f\n", (double)latitude)) {
            FURI_LOG_E(TAG, "Unable to add Lat");
            break;
        }
        if(!stream_insert_format(stream, "Lon: %f\n", (double)longitude)) {
            FURI_LOG_E(TAG, "Unable to add Lon");
            break;
        }
    } while(false);

    furi_check(
        decoder_base->protocol && decoder_base->protocol->decoder &&
        decoder_base->protocol->decoder->get_string);
    decoder_base->protocol->decoder->get_string(decoder_base, item->item_str);
    instance->last_index_write++;
    return true;
}

void subghz_wardriving_history_remove_duplicates(SubGhzHistory* instance) {
    furi_assert(instance);

    // Keep the newest occurrence of every signal. m-lib shifts down only what sits above
    // the removed slot, so deleting below the cursor keeps the lower indexes valid - but
    // the kept item slides down with it, hence the i--
    size_t i = SubGhzHistoryItemArray_size(instance->history->data);
    while(i > 0) {
        i--;
        SubGhzHistoryItem* keep = SubGhzHistoryItemArray_get(instance->history->data, i);
        const uint32_t hash_data = keep->hash_data;
        const SubGhzProtocol* protocol = keep->protocol;

        size_t j = i;
        while(j > 0) {
            j--;
            SubGhzHistoryItem* item = SubGhzHistoryItemArray_get(instance->history->data, j);
            if((item->hash_data == hash_data) && (item->protocol == protocol)) {
                subghz_wardriving_history_delete_item(instance, (uint16_t)j);
                i--;
            }
        }
    }
}

void subghz_wardriving_history_set_saved(SubGhzHistory* instance, uint16_t idx, bool saved) {
    furi_assert(instance);
    if(idx < SubGhzHistoryItemArray_size(instance->history->data)) {
        SubGhzHistoryItemArray_get(instance->history->data, idx)->saved = saved;
    }
}

int32_t subghz_wardriving_history_get_unsaved(SubGhzHistory* instance) {
    furi_assert(instance);
    const size_t size = SubGhzHistoryItemArray_size(instance->history->data);
    for(size_t i = 0; i < size; i++) {
        if(!SubGhzHistoryItemArray_get(instance->history->data, i)->saved) return (int32_t)i;
    }
    return -1;
}


