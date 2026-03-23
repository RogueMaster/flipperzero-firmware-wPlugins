#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include "../metroflip_i.h"
#include "desfire.h"
#include <string.h>
#include <applications/services/storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#define DESFIRE_CARDS_PATH APP_ASSETS_PATH("desfire/cards.txt")

// Parse a 6-char hex AID string into a 3-byte big-endian array
static bool parse_aid_hex(const char* hex, uint8_t out[3]) {
    for(int i = 0; i < 3; i++) {
        uint8_t hi = hex[i * 2];
        uint8_t lo = hex[i * 2 + 1];
        hi = (hi >= 'A') ? (hi - 'A' + 10) : (hi >= 'a') ? (hi - 'a' + 10) : (hi - '0');
        lo = (lo >= 'A') ? (lo - 'A' + 10) : (lo >= 'a') ? (lo - 'a' + 10) : (lo - '0');
        if(hi > 15 || lo > 15) return false;
        out[i] = (hi << 4) | lo;
    }
    return true;
}

// Search the asset file for a matching AID. Returns card_name via out_name (static buffer).
// Returns locked status. Returns false if no match found.
static bool desfire_lookup_aid(
    const uint8_t aid[3],
    char* out_name,
    size_t name_size,
    bool* out_locked) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool found = false;

    if(file_stream_open(stream, DESFIRE_CARDS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            const char* str = furi_string_get_cstr(line);
            // Format: AAAAAA,card_name,locked
            if(furi_string_size(line) < 8) continue;

            uint8_t file_aid[3];
            if(!parse_aid_hex(str, file_aid)) continue;
            if(memcmp(file_aid, aid, 3) != 0) continue;

            // Found match — extract card name and locked flag
            const char* name_start = strchr(str, ',');
            if(!name_start) continue;
            name_start++;

            const char* name_end = strrchr(str, ',');
            if(!name_end || name_end <= name_start) continue;

            size_t len = name_end - name_start;
            if(len >= name_size) len = name_size - 1;
            memcpy(out_name, name_start, len);
            out_name[len] = '\0';

            // Trim trailing whitespace
            while(len > 0 && (out_name[len - 1] == '\r' || out_name[len - 1] == '\n')) {
                out_name[--len] = '\0';
            }

            char locked_ch = *(name_end + 1);
            *out_locked = (locked_ch == '1');
            found = true;
            break;
        }
        furi_string_free(line);
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return found;
}

const char* desfire_type(const MfDesfireData* data) {
    if(!data || !data->application_ids) return "Unknown Card";

    const uint32_t count = simple_array_get_count(data->application_ids);

    // Static buffer to hold the matched card name across function calls.
    // Caller uses the pointer until next call to desfire_type.
    static char matched_name[64];
    bool locked;

    for(uint32_t j = 0; j < count; j++) {
        const MfDesfireApplicationId* app =
            (const MfDesfireApplicationId*)simple_array_cget(data->application_ids, j);
        if(!app) continue;

        if(desfire_lookup_aid(app->data, matched_name, sizeof(matched_name), &locked)) {
            return matched_name;
        }
    }

    return "Unknown Card";
}

bool is_desfire_locked(const char* card_name) {
    if(!card_name) return true;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool locked = true;

    if(file_stream_open(stream, DESFIRE_CARDS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            const char* str = furi_string_get_cstr(line);
            const char* name_start = strchr(str, ',');
            if(!name_start) continue;
            name_start++;
            const char* name_end = strrchr(str, ',');
            if(!name_end || name_end <= name_start) continue;

            size_t len = name_end - name_start;
            if(strlen(card_name) == len && strncmp(card_name, name_start, len) == 0) {
                locked = (*(name_end + 1) == '1');
                break;
            }
        }
        furi_string_free(line);
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return locked;
}
