#include "pocsag_pager_history.h"
#include "protocols/pocsag.h"
#include "protocols/pcsg_generic.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/subghz/receiver.h>
#include <m-array.h>
#include <stdio.h>
#include <string.h>

#define PCSG_HISTORY_MAX     100U
#define PCSG_PREVIEW_MAX_LEN 40U
#define PCSG_RX_CACHE_DIR    APP_DATA_PATH("rx_cache")
#define TAG                  "PCSGHistory"

// Minimal RAM footprint per message (~52 bytes); full payload lives on microSD.
typedef struct {
    uint16_t file_id;
    uint32_t frequency;
    uint8_t type;
    bool on_sd;
    char preview[PCSG_PREVIEW_MAX_LEN];
} PCSGHistoryItem;

ARRAY_DEF(PCSGHistoryItemArray, PCSGHistoryItem, M_POD_OPLIST)
#define M_OPL_PCSGHistoryItemArray_t() ARRAY_OPLIST(PCSGHistoryItemArray, M_POD_OPLIST)

struct PCSGHistory {
    uint32_t last_update_timestamp;
    uint16_t last_index_write;
    uint16_t next_file_id;
    uint8_t code_last_hash_data;
    PCSGHistoryItemArray_t data;
};

static void pcsg_history_make_path(uint16_t file_id, char* buf, size_t buf_size) {
    snprintf(buf, buf_size, "%s/%04u.txt", PCSG_RX_CACHE_DIR, (unsigned)file_id);
}

static void pcsg_history_clear_cache_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove_recursive(storage, PCSG_RX_CACHE_DIR);
    furi_record_close(RECORD_STORAGE);
}

static void pcsg_history_strip_escapes(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if(src[i] == '\e' && src[i + 1] != '\0') {
            i++;
            continue;
        }
        char c = src[i];
        if(c == '\r' || c == '\n') c = ' ';
        dst[j++] = c;
    }
    dst[j] = '\0';
}

static bool pcsg_history_write_sd(
    uint16_t file_id,
    uint32_t frequency,
    const char* ric_header,
    const char* message_body) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    storage_simply_mkdir(storage, PCSG_RX_CACHE_DIR);

    char path[64];
    pcsg_history_make_path(file_id, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Line 1: "[P1200] RIC: 123456 | Msg (439.9875 MHz)"
        char clean_hdr[96];
        pcsg_history_strip_escapes(ric_header ? ric_header : "", clean_hdr, sizeof(clean_hdr));
        size_t hdr_len = strlen(clean_hdr);
        while(hdr_len > 0 && (clean_hdr[hdr_len - 1] == ' ' || clean_hdr[hdr_len - 1] == ':')) {
            clean_hdr[--hdr_len] = '\0';
        }

        char line1[128];
        int n = snprintf(
            line1,
            sizeof(line1),
            "%s  %lu.%04lu MHz\n",
            clean_hdr,
            (unsigned long)(frequency / 1000000U),
            (unsigned long)((frequency % 1000000U) / 100U));

        bool hdr_ok = (n > 0) && (storage_file_write(file, line1, (size_t)n) == (size_t)n);
        size_t body_len = message_body ? strlen(message_body) : 0;
        bool body_ok = (body_len == 0) ||
                       (storage_file_write(file, message_body, body_len) == body_len);
        ok = hdr_ok && body_ok;
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

PCSGHistory* pcsg_history_alloc(void) {
    PCSGHistory* instance = malloc(sizeof(PCSGHistory));
    memset(instance, 0, sizeof(PCSGHistory));
    PCSGHistoryItemArray_init(instance->data);
    pcsg_history_clear_cache_dir();
    return instance;
}

void pcsg_history_free(PCSGHistory* instance) {
    furi_assert(instance);
    PCSGHistoryItemArray_clear(instance->data);
    pcsg_history_clear_cache_dir();
    free(instance);
}

void pcsg_history_reset(PCSGHistory* instance) {
    furi_assert(instance);
    PCSGHistoryItemArray_reset(instance->data);
    instance->last_index_write = 0;
    instance->next_file_id = 0;
    instance->code_last_hash_data = 0;
    pcsg_history_clear_cache_dir();
}

uint16_t pcsg_history_get_item(PCSGHistory* instance) {
    furi_assert(instance);
    return instance->last_index_write;
}

uint8_t pcsg_history_get_type_protocol(PCSGHistory* instance, uint16_t idx) {
    furi_assert(instance);
    PCSGHistoryItem* item = PCSGHistoryItemArray_get(instance->data, idx);
    return item->type;
}

uint32_t pcsg_history_get_frequency(PCSGHistory* instance, uint16_t idx) {
    furi_assert(instance);
    PCSGHistoryItem* item = PCSGHistoryItemArray_get(instance->data, idx);
    return item->frequency;
}

bool pcsg_history_get_text_space_left(PCSGHistory* instance, FuriString* output) {
    furi_assert(instance);
    if(instance->last_index_write >= PCSG_HISTORY_MAX) {
        if(output != NULL) furi_string_printf(output, "History FULL");
        return true;
    }
    if(output != NULL) {
        furi_string_printf(output, "Rx:%u", instance->last_index_write);
    }
    return false;
}

void pcsg_history_get_text_item_menu(PCSGHistory* instance, FuriString* output, uint16_t idx) {
    furi_assert(instance);
    PCSGHistoryItem* item = PCSGHistoryItemArray_get(instance->data, idx);
    furi_string_set_str(output, item->preview);
}

PCSGHistoryStateAddKey
    pcsg_history_add_to_history(PCSGHistory* instance, void* context, SubGhzRadioPreset* preset) {
    furi_assert(instance);
    furi_assert(context);

    if(instance->last_index_write >= PCSG_HISTORY_MAX) return PCSGHistoryStateAddKeyOverflow;

    SubGhzProtocolDecoderBase* decoder_base = context;
    uint8_t hash = subghz_protocol_decoder_base_get_hash_data(decoder_base);
    if((instance->code_last_hash_data == hash) &&
       ((furi_get_tick() - instance->last_update_timestamp) < 500)) {
        instance->last_update_timestamp = furi_get_tick();
        return PCSGHistoryStateAddKeyTimeOut;
    }

    instance->code_last_hash_data = hash;
    instance->last_update_timestamp = furi_get_tick();

    const char* ric_cstr = "";
    const char* msg_cstr = "";
    if(decoder_base->protocol == &subghz_protocol_pocsag) {
        pocsag_decoder_get_decoded_parts(decoder_base, &ric_cstr, &msg_cstr);
    }

    PCSGHistoryItem* item = PCSGHistoryItemArray_push_raw(instance->data);
    memset(item, 0, sizeof(PCSGHistoryItem));
    item->file_id = instance->next_file_id++;
    item->frequency = preset ? preset->frequency : 0;
    item->type = decoder_base->protocol->type;

    // History list shows only the message text (falls back to RIC/Alert for tone-only)
    char clean_msg[PCSG_PREVIEW_MAX_LEN];
    pcsg_history_strip_escapes(msg_cstr, clean_msg, sizeof(clean_msg));
    const char* trimmed = clean_msg;
    while(*trimmed == ' ')
        trimmed++;
    if(*trimmed != '\0') {
        strlcpy(item->preview, trimmed, sizeof(item->preview));
    } else {
        pcsg_history_strip_escapes(ric_cstr, item->preview, sizeof(item->preview));
    }

    // Offload full message payload straight to microSD
    item->on_sd = pcsg_history_write_sd(item->file_id, item->frequency, ric_cstr, msg_cstr);

    instance->last_index_write++;
    return PCSGHistoryStateAddKeyNewDada;
}

bool pcsg_history_load_message(
    PCSGHistory* instance,
    uint16_t idx,
    FuriString* out_header,
    FuriString* out_body) {
    furi_assert(instance);
    furi_assert(out_header);
    furi_assert(out_body);

    furi_string_reset(out_header);
    furi_string_reset(out_body);

    if(idx >= instance->last_index_write) return false;
    PCSGHistoryItem* item = PCSGHistoryItemArray_get(instance->data, idx);

    if(!item->on_sd) {
        furi_string_printf(
            out_header,
            "%lu.%04lu MHz",
            (unsigned long)(item->frequency / 1000000U),
            (unsigned long)((item->frequency % 1000000U) / 100U));
        furi_string_set_str(out_body, item->preview);
        return true;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char path[64];
    pcsg_history_make_path(item->file_id, path, sizeof(path));

    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[128];
        bool in_header = true;
        size_t read_bytes = 0;
        while((read_bytes = storage_file_read(file, buf, sizeof(buf))) > 0) {
            for(size_t i = 0; i < read_bytes; i++) {
                char c = buf[i];
                if(in_header) {
                    if(c == '\n') {
                        in_header = false;
                    } else {
                        furi_string_push_back(out_header, c);
                    }
                } else {
                    furi_string_push_back(out_body, c);
                }
            }
        }
        storage_file_close(file);
        ok = true;
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!ok) {
        furi_string_set_str(out_header, "SD read error");
        furi_string_set_str(out_body, item->preview);
    }
    return ok;
}
