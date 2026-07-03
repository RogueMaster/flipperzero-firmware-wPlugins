#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "../../api/metroflip/metroflip_api.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <storage/storage.h>
#include <ctype.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:RenfeRegular"

// ---------------------------------------------------------------------------
// LSB-first bit reader (matches Renfe card encoding)
// ---------------------------------------------------------------------------

static uint32_t renfe_read_bits(const uint8_t* data, uint16_t start, uint8_t n) {
    uint32_t v = 0;
    for(uint8_t i = 0; i < n; i++) {
        uint16_t bp = start + i;
        uint8_t byte_idx = bp >> 3;
        uint8_t bit_idx = bp & 7;
        if(data[byte_idx] & (1 << bit_idx)) {
            v |= (1UL << i);
        }
    }
    return v;
}

// Decode a 15-bit DMY date: year(6, +2000) | month(4) | day(5)
// Returns true if valid, fills y/m/d
static bool renfe_decode_date(const uint8_t* data, uint16_t start, int* y, int* m, int* d) {
    uint32_t val = renfe_read_bits(data, start, 15);
    *y = (int)(val & 0x3F) + 2000;
    *m = (int)((val >> 6) & 0xF);
    *d = (int)((val >> 10) & 0x1F);
    if(*m < 1 || *m > 12 || *d < 1 || *d > 31) return false;
    if(*y < 2000 || *y > 2050) return false;
    return true;
}

// Decode 11-bit time (minutes since midnight)
static bool renfe_decode_time(const uint8_t* data, uint16_t start, int* h, int* m) {
    uint32_t val = renfe_read_bits(data, start, 11);
    if(val >= 1440) return false;
    *h = (int)(val / 60);
    *m = (int)(val % 60);
    return true;
}

// ---------------------------------------------------------------------------
// Station name cache
// ---------------------------------------------------------------------------

#define MAX_STATION_NAME_LENGTH 28
#define MAX_CACHED_STATIONS     50

typedef struct {
    uint16_t code;
    char name[MAX_STATION_NAME_LENGTH];
} StationEntry;

static struct {
    StationEntry stations[MAX_CACHED_STATIONS];
    size_t count;
    bool loaded;
    char current_region[16];
} station_cache = {0};

static void renfe_regular_clear_station_cache(void) {
    station_cache.count = 0;
    station_cache.loaded = false;
    memset(station_cache.current_region, 0, sizeof(station_cache.current_region));
    memset(station_cache.stations, 0, sizeof(station_cache.stations));
}

static bool renfe_regular_load_station_file(const char* region) {
    if(!region) return false;
    if(station_cache.loaded && strcmp(station_cache.current_region, region) == 0) return true;

    renfe_regular_clear_station_cache();

    FuriString* file_path = furi_string_alloc();
    furi_string_printf(file_path, "/ext/apps_assets/metroflip/renfe/stations/%s.txt", region);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line_buffer[64];
        while(station_cache.count < MAX_CACHED_STATIONS) {
            size_t line_pos = 0;
            bool end_of_file = false;
            while(line_pos < sizeof(line_buffer) - 1) {
                char c;
                if(storage_file_read(file, &c, 1) == 0) {
                    end_of_file = true;
                    break;
                }
                if(c == '\n') break;
                if(c != '\r') line_buffer[line_pos++] = c;
            }
            if(end_of_file && line_pos == 0) break;
            line_buffer[line_pos] = '\0';
            if(line_buffer[0] == '#' || line_buffer[0] == '\0' || strlen(line_buffer) < 3) {
                if(end_of_file) break;
                continue;
            }
            char* comma = strchr(line_buffer, ',');
            if(comma) {
                *comma = '\0';
                uint16_t code = 0;
                if(sscanf(line_buffer, "0x%hX", &code) == 1) {
                    station_cache.stations[station_cache.count].code = code;
                    strncpy(
                        station_cache.stations[station_cache.count].name,
                        comma + 1,
                        MAX_STATION_NAME_LENGTH - 1);
                    station_cache.stations[station_cache.count].name[MAX_STATION_NAME_LENGTH - 1] =
                        '\0';
                    station_cache.count++;
                }
            }
            if(end_of_file) break;
        }
        if(station_cache.count > 0) {
            strncpy(
                station_cache.current_region, region, sizeof(station_cache.current_region) - 1);
            station_cache.current_region[sizeof(station_cache.current_region) - 1] = '\0';
            station_cache.loaded = true;
            success = true;
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(file_path);
    return success;
}

static const char* renfe_regular_get_station_name(uint16_t station_code, const char* region) {
    if(station_code == 0x0000 || station_code == 0xFFFF) return NULL;
    if(region && renfe_regular_load_station_file(region)) {
        for(size_t i = 0; i < station_cache.count; i++) {
            if(station_cache.stations[i].code == station_code)
                return station_cache.stations[i].name;
        }
    }
    if(renfe_regular_load_station_file("general")) {
        for(size_t i = 0; i < station_cache.count; i++) {
            if(station_cache.stations[i].code == station_code)
                return station_cache.stations[i].name;
        }
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Card type / region / trip detection
// ---------------------------------------------------------------------------

static const char* renfe_regular_detect_card_type(const MfClassicData* data) {
    if(!data) return "Unknown";
    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* b12 = data->block[12].data;
        if(b12[0] == 0xE8 && b12[1] == 0x03) return "Bono 10 trips";
        if((b12[0] == 0xE4 && b12[1] == 0x02) || (b12[0] == 0x04 && b12[1] == 0x01))
            return "Ida/Vuelta";
        uint16_t price = (uint16_t)b12[0] | ((uint16_t)b12[1] << 8);
        if(price > 0x0500) return "Long Distance";
    }
    if(mf_classic_is_block_read(data, 2)) {
        const uint8_t* b2 = data->block[2].data;
        if(b2[2] == 0x5C && b2[3] == 0x9F) return "Cercanias";
    }
    if(mf_classic_is_block_read(data, 8)) {
        const uint8_t* b8 = data->block[8].data;
        if(b8[0] == 0x81 && b8[1] == 0xE2) return "Regular";
    }
    return "Tarjeta Regular";
}

static const char* renfe_regular_get_region(const MfClassicData* data) {
    if(!data) return "unknown";

    if(mf_classic_is_block_read(data, 2)) {
        const uint8_t* b2 = data->block[2].data;
        if(b2[2] == 0x5C && b2[3] == 0x9F) return "valencia";
        if(b2[2] == 0x5D && b2[3] == 0xA0) return "madrid";
        if(b2[2] == 0x5A && b2[3] == 0x9D) return "cataluna";
        if(b2[2] == 0x5E && b2[3] == 0xA1) return "andalucia";
        if(b2[2] == 0x5B && b2[3] == 0x9E) return "pais_vasco";
    }

    if(mf_classic_is_block_read(data, 12)) {
        const uint8_t* b12 = data->block[12].data;
        if(b12[0] == 0xE3 && b12[1] == 0x01) return "aragon";
        if(b12[0] == 0xE7 && (b12[1] == 0x03 || b12[1] == 0x04)) return "castilla_leon";
        if(b12[0] == 0xE8 && b12[1] == 0x05) return "galicia";
        if(b12[0] == 0xE9 && b12[1] == 0x06) return "asturias";
        if(b12[0] == 0xEA && b12[1] == 0x07) return "pais_vasco";
        if(b12[0] == 0xEB && b12[1] == 0x08) return "cantabria";
        if(b12[0] == 0xE4 && b12[1] == 0x02) {
            uint16_t code = ((uint16_t)b12[10] << 8) | b12[11];
            if(code >= 0x1000 && code <= 0x1FFF) return "madrid";
        }
        if(b12[2] == 0x02 && b12[3] == 0x00) return "cataluna";
        if(b12[0] == 0xE5 || b12[0] == 0xE6) return "andalucia";
    }

    if(mf_classic_is_block_read(data, 13)) {
        const uint8_t* b13 = data->block[13].data;
        if(b13[6] == 0x4D && b13[7] == 0xDF) return "pais_vasco";
    }

    if(mf_classic_is_block_read(data, 1)) {
        const uint8_t* b1 = data->block[1].data;
        if(b1[12] == 0xCA && b1[13] == 0xAA) return "cataluna";
        if(b1[12] == 0xBA && b1[13] == 0xEE) return "valencia";
    }

    return "unknown";
}

static void renfe_format_region(const char* region, char* out, size_t out_len) {
    strncpy(out, region, out_len - 1);
    out[out_len - 1] = '\0';
    if(strlen(out) > 0) {
        out[0] = (char)toupper((unsigned char)out[0]);
        for(size_t i = 1; i < strlen(out); i++) {
            if(out[i] == '_') {
                out[i] = ' ';
                if(i + 1 < strlen(out)) {
                    out[i + 1] = (char)toupper((unsigned char)out[i + 1]);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Trip status / count
// ---------------------------------------------------------------------------

static void renfe_get_trip_status(const MfClassicData* data, char* out, size_t out_len) {
    if(!data || !mf_classic_is_block_read(data, 12)) {
        snprintf(out, out_len, "N/A");
        return;
    }
    const uint8_t* b12 = data->block[12].data;
    if(b12[0] == 0xE8 && b12[1] == 0x03) {
        int c = (int)b12[2];
        if(c >= 0 && c <= 10)
            snprintf(out, out_len, "%d/10", c);
        else
            snprintf(out, out_len, "?/10");
    } else if((b12[0] == 0xE4 && b12[1] == 0x02) || (b12[0] == 0x04 && b12[1] == 0x01)) {
        if(b12[2] == 0x02)
            snprintf(out, out_len, "Used");
        else if(b12[2] == 0x00)
            snprintf(out, out_len, "Unused");
        else
            snprintf(out, out_len, "0x%02X", b12[2]);
    } else {
        int c = (int)b12[2];
        if(c >= 0 && c <= 10)
            snprintf(out, out_len, "%d trips", c);
        else
            snprintf(out, out_len, "0x%02X", c);
    }
}

// ---------------------------------------------------------------------------
// History entry detection and classification
// ---------------------------------------------------------------------------

static bool renfe_is_history_entry(const uint8_t* bd) {
    if(!bd) return false;
    // Check for all-zero or all-FF blocks
    bool all_empty = true;
    for(int i = 0; i < 16; i++) {
        if(bd[i] != 0x00 && bd[i] != 0xFF) {
            all_empty = false;
            break;
        }
    }
    if(all_empty) return false;

    // Primary check: byte 2 is A6 or DE, and byte 7 is 0x10
    if((bd[2] == 0xA6 || bd[2] == 0xDE) && bd[7] == 0x10) {
        return true;
    }
    return false;
}

static const char* renfe_classify_tx(const uint8_t* bd) {
    // Byte 2: A6 = entry/validation side, DE = exit/transfer side
    if(bd[2] == 0xA6) {
        return (bd[0] & 0x01) ? "Entry" : "Check";
    }
    if(bd[2] == 0xDE) {
        return (bd[0] & 0x01) ? "Transfer" : "Exit";
    }
    return "Transaction";
}

// ---------------------------------------------------------------------------
// Keys
// ---------------------------------------------------------------------------

const MfClassicKeyPair renfe_regular_keys[16] = {
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 0
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 1
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 2
    {.a = 0x747734CC8ED3, .b = 0x78778869ffff}, // Sector 3
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 4
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 5
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 6
    {.a = 0xffffffffffff, .b = 0x78778869ffff}, // Sector 7
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 8
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 9
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 10
    {.a = 0x749934CC8ED3, .b = 0x78778869ffff}, // Sector 11
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 12
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 13
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 14
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 15
};

typedef struct {
    uint8_t data_sector;
    const MfClassicKeyPair* keys;
} RenfeRegularCardConfig;

static bool renfe_regular_get_card_config(RenfeRegularCardConfig* config, MfClassicType type) {
    if(type != MfClassicType1k && type != MfClassicType4k) return false;
    config->data_sector = 5;
    config->keys = renfe_regular_keys;
    return true;
}

// ---------------------------------------------------------------------------
// Display card view
// ---------------------------------------------------------------------------

static bool
    renfe_regular_display_card_view(const MfClassicData* data, Metroflip* app, bool from_file) {
    if(!data) return false;

    RenfeRegularCardConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    if(!renfe_regular_get_card_config(&cfg, data->type)) return false;

    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "RENFE Regular");

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];
    const char* region = renfe_regular_get_region(data);

    // =====================================================================
    // Page 1: Card Info
    // =====================================================================
    uint8_t p = metroflip_card_view_add_page(view, "Card Info");
    uint8_t info_page = p;

    const char* card_type = renfe_regular_detect_card_type(data);
    snprintf(val, sizeof(val), "%.23s", card_type);
    metroflip_card_view_add_field(view, p, "Type", val, false);

    renfe_get_trip_status(data, val, sizeof(val));
    bool is_bono =
        (mf_classic_is_block_read(data, 12) && data->block[12].data[0] == 0xE8 &&
         data->block[12].data[1] == 0x03);
    metroflip_card_view_add_field(view, p, is_bono ? "Trips" : "Status", val, true);

    metroflip_card_view_add_field(
        view,
        p,
        "Format",
        (data->type == MfClassicType1k) ? "Mifare Classic 1K" : "Mifare Classic 4K",
        false);

    if(data->iso14443_3a_data && data->iso14443_3a_data->uid_len > 0) {
        const uint8_t* uid = data->iso14443_3a_data->uid;
        size_t uid_len = data->iso14443_3a_data->uid_len;
        size_t pos = 0;
        val[0] = '\0';
        for(size_t i = 0; i < uid_len && pos < sizeof(val) - 3; i++) {
            pos += (size_t)snprintf(val + pos, sizeof(val) - pos, i ? " %02X" : "%02X", uid[i]);
        }
        metroflip_card_view_add_field(view, p, "UID", val, false);
    } else if(mf_classic_is_block_read(data, 0)) {
        const uint8_t* b0 = data->block[0].data;
        snprintf(val, sizeof(val), "%02X %02X %02X %02X", b0[0], b0[1], b0[2], b0[3]);
        metroflip_card_view_add_field(view, p, "UID", val, false);
    } else {
        metroflip_card_view_add_field(view, p, "UID", "N/A", false);
    }

    char disp_region[24];
    renfe_format_region(region, disp_region, sizeof(disp_region));
    metroflip_card_view_add_field(view, p, "Region", disp_region, false);

    // =====================================================================
    // Page 2: Card Dates (Block 8)
    // =====================================================================
    if(mf_classic_is_block_read(data, 8)) {
        const uint8_t* b8 = data->block[8].data;
        int y, m, d;

        p = metroflip_card_view_add_page(view, "Card Dates");

        if(renfe_decode_date(b8, 14, &y, &m, &d)) {
            snprintf(val, sizeof(val), "%04d-%02d-%02d", y, m, d);
            metroflip_card_view_add_field(view, p, "Start", val, false);
        }

        if(renfe_decode_date(b8, 29, &y, &m, &d)) {
            snprintf(val, sizeof(val), "%04d-%02d-%02d", y, m, d);
            metroflip_card_view_add_field(view, p, "Expiry", val, false);
        }

        // Price from Block 12
        if(mf_classic_is_block_read(data, 12)) {
            const uint8_t* b12 = data->block[12].data;
            uint16_t price = (uint16_t)b12[0] | ((uint16_t)b12[1] << 8);
            if(price > 0 && price < 50000) {
                snprintf(val, sizeof(val), "%d.%02d EUR", price / 100, price % 100);
                metroflip_card_view_add_field(view, p, "Price", val, true);
            }
        }

        uint32_t tariff = renfe_read_bits(b8, 56, 10);
        if(tariff > 0) {
            snprintf(val, sizeof(val), "%lu", (unsigned long)tariff);
            metroflip_card_view_add_field(view, p, "Tariff", val, false);
        }
    }

    // =====================================================================
    // Page 3: Title Info (Block 13) + Last Charge (Block 61)
    // =====================================================================
    {
        bool has_title_data = false;
        p = metroflip_card_view_add_page(view, "Title / Charge");

        if(mf_classic_is_block_read(data, 13)) {
            const uint8_t* b13 = data->block[13].data;
            int y, m, d;
            if(renfe_decode_date(b13, 0, &y, &m, &d)) {
                snprintf(val, sizeof(val), "%04d-%02d-%02d", y, m, d);
                metroflip_card_view_add_field(view, p, "Title Start", val, false);
                has_title_data = true;
            }
        }

        // Block 61: charge/sale history
        if(mf_classic_is_block_read(data, 61)) {
            const uint8_t* b61 = data->block[61].data;
            // Check not empty
            bool b61_empty = true;
            for(int i = 0; i < 8; i++) {
                if(b61[i] != 0x00 && b61[i] != 0xFF) {
                    b61_empty = false;
                    break;
                }
            }
            if(!b61_empty) {
                int y, m, d;
                if(renfe_decode_date(b61, 86, &y, &m, &d)) {
                    int th, tm;
                    if(renfe_decode_time(b61, 101, &th, &tm)) {
                        snprintf(val, sizeof(val), "%04d-%02d-%02d %02d:%02d", y, m, d, th, tm);
                    } else {
                        snprintf(val, sizeof(val), "%04d-%02d-%02d", y, m, d);
                    }
                    metroflip_card_view_add_field(view, p, "Last Charge", val, false);
                    has_title_data = true;
                }

                // Terminal ID
                char letter = (char)b61[0];
                if(letter >= 0x20 && letter <= 0x7E) {
                    uint32_t pupitre_num = renfe_read_bits(b61, 8, 13);
                    snprintf(val, sizeof(val), "%c-%lu", letter, (unsigned long)pupitre_num);
                    metroflip_card_view_add_field(view, p, "Terminal", val, false);
                    has_title_data = true;
                }
            }
        }

        if(!has_title_data) {
            metroflip_card_view_add_field(view, p, "Data", "N/A", false);
        }
    }

    // =====================================================================
    // Page 4: Sale History (Block 57) — if present
    // =====================================================================
    if(mf_classic_is_block_read(data, 57)) {
        const uint8_t* b57 = data->block[57].data;
        bool b57_empty = true;
        for(int i = 0; i < 8; i++) {
            if(b57[i] != 0x00 && b57[i] != 0xFF) {
                b57_empty = false;
                break;
            }
        }
        if(!b57_empty) {
            int y, m, d;
            if(renfe_decode_date(b57, 86, &y, &m, &d)) {
                p = metroflip_card_view_add_page(view, "Sale History");
                int th, tm;
                if(renfe_decode_time(b57, 101, &th, &tm)) {
                    snprintf(val, sizeof(val), "%04d-%02d-%02d %02d:%02d", y, m, d, th, tm);
                } else {
                    snprintf(val, sizeof(val), "%04d-%02d-%02d", y, m, d);
                }
                metroflip_card_view_add_field(view, p, "Sale Date", val, false);

                char letter = (char)b57[0];
                if(letter >= 0x20 && letter <= 0x7E) {
                    uint32_t pnum = renfe_read_bits(b57, 8, 13);
                    snprintf(val, sizeof(val), "%c-%lu", letter, (unsigned long)pnum);
                    metroflip_card_view_add_field(view, p, "Terminal", val, false);
                }

                uint32_t tariff = renfe_read_bits(b57, 76, 10);
                if(tariff > 0) {
                    snprintf(val, sizeof(val), "%lu", (unsigned long)tariff);
                    metroflip_card_view_add_field(view, p, "Tariff", val, false);
                }
            }
        }
    }

    // =====================================================================
    // Trip history pages
    // =====================================================================
    {
        // History blocks live in sectors 4-5 (blocks 16-22) and 10-12 (blocks 40-50)
        static const int history_blocks[] = {18, 20, 21, 40, 41, 42, 44, 45, 46, 48};
        int num_history = (int)(sizeof(history_blocks) / sizeof(history_blocks[0]));
        int max_block = (data->type == MfClassicType1k) ? 64 : 256;
        int trip_num = 0;

        for(int i = 0; i < num_history && trip_num < 10; i++) {
            int bn = history_blocks[i];
            if(bn >= max_block) continue;
            if(!mf_classic_is_block_read(data, bn)) continue;

            const uint8_t* bd = data->block[bn].data;
            if(!renfe_is_history_entry(bd)) continue;

            trip_num++;
            const char* tx_type = renfe_classify_tx(bd);

            char hdr[METROFLIP_CARD_VIEW_HEADER_LEN];
            snprintf(hdr, sizeof(hdr), "Trip %d - %s", trip_num, tx_type);

            uint8_t hp = metroflip_card_view_add_page(view, hdr);
            if(hp == UINT8_MAX) break;

            // Time of day: byte5 | ((byte6 & 0x07) << 8), mod 1440
            {
                uint16_t raw_time = (uint16_t)bd[5] | (((uint16_t)bd[6] & 0x07) << 8);
                if(raw_time >= 1440) raw_time = raw_time % 1440;
                int th = raw_time / 60;
                int tm = raw_time % 60;
                snprintf(val, sizeof(val), "%02d:%02d", th, tm);
                metroflip_card_view_add_field(view, hp, "Time", val, false);
            }

            // Gate direction
            if(bd[8] == 0x10) {
                metroflip_card_view_add_field(view, hp, "Gate", "IN", false);
            } else if(bd[8] == 0x00) {
                metroflip_card_view_add_field(view, hp, "Gate", "OUT", false);
            }

            // Zone (byte 11, often 0x0C = zone 12)
            if(bd[11] > 0) {
                snprintf(val, sizeof(val), "%d", bd[11]);
                metroflip_card_view_add_field(view, hp, "Zone", val, false);
            }

            // Station lookup (bytes 9-10 BE)
            uint16_t station_code = ((uint16_t)bd[9] << 8) | bd[10];
            if(station_code != 0x0000 && station_code != 0xFFFF) {
                const char* sname = renfe_regular_get_station_name(station_code, region);
                if(sname) {
                    snprintf(val, sizeof(val), "%.23s", sname);
                    metroflip_card_view_add_field(view, hp, "Station", val, false);
                } else {
                    snprintf(val, sizeof(val), "0x%04X", station_code);
                    metroflip_card_view_add_field(view, hp, "Station", val, false);
                }
            }

            // Fare detail (byte 7: 0x10 = standard fare)
            if(bd[7] != 0x00 && bd[7] != 0xFF) {
                metroflip_card_view_add_field(
                    view, hp, "Fare", (bd[7] == 0x10) ? "Standard" : "Special", false);
            }

            // Source block of this entry
            snprintf(val, sizeof(val), "%d", bn);
            metroflip_card_view_add_field(view, hp, "Block", val, false);
        }

        // Bono cards with no decodable history: show raw trip data blocks
        if(is_bono && trip_num == 0) {
            static const int bono_raw_blocks[] = {4, 5, 6, 7, 8};
            uint8_t rp = metroflip_card_view_add_page(view, "Bono Raw Data");
            if(rp != UINT8_MAX) {
                metroflip_card_view_add_field(
                    view, rp, "", "History format not yet decoded", false);
                char lbl[METROFLIP_CARD_VIEW_LABEL_LEN];
                for(size_t i = 0; i < sizeof(bono_raw_blocks) / sizeof(bono_raw_blocks[0]); i++) {
                    int bn = bono_raw_blocks[i];
                    if(bn >= max_block || !mf_classic_is_block_read(data, bn)) continue;
                    const uint8_t* bd = data->block[bn].data;
                    snprintf(lbl, sizeof(lbl), "B%02d", bn);
                    for(int j = 0; j < 8; j++) {
                        snprintf(val + j * 2, 3, "%02X", bd[j]);
                    }
                    metroflip_card_view_add_field(view, rp, lbl, val, false);
                    for(int j = 0; j < 8; j++) {
                        snprintf(val + j * 2, 3, "%02X", bd[j + 8]);
                    }
                    metroflip_card_view_add_field(view, rp, "", val, false);
                }
            }
        }

        // History availability status (not shown for Ida/Vuelta cards)
        bool is_ida_vuelta =
            (mf_classic_is_block_read(data, 12) &&
             ((data->block[12].data[0] == 0xE4 && data->block[12].data[1] == 0x02) ||
              (data->block[12].data[0] == 0x04 && data->block[12].data[1] == 0x01)));
        if(!is_ida_vuelta) {
            metroflip_card_view_add_field(
                view, info_page, "History", (trip_num > 0) ? "Available" : "Empty", false);
        }
    }

    // Buttons
    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

// ---------------------------------------------------------------------------
// Plugin lifecycle
// ---------------------------------------------------------------------------

static void renfe_regular_on_enter(Metroflip* app) {
    if(!app) return;

    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        MfClassicData* mfc_data = NULL;
        bool should_free = false;

        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            should_free = true;
        }
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);

        if(mfc_data) {
            if(!renfe_regular_display_card_view(mfc_data, app, true)) {
                Widget* widget = app->widget;
                FuriString* s = furi_string_alloc_set("\e#Unknown card\n");
                widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                furi_string_free(s);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            }
            if(should_free) mf_classic_free(mfc_data);
        } else {
            FURI_LOG_E(TAG, "Failed to open saved file: %s", app->file_path);
            Widget* widget = app->widget;
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, "\e#Error\nFailed to open\nsaved file.");
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
    } else {
        Widget* widget = app->widget;
        if(!widget || !app->view_dispatcher) return;

        FuriString* message = furi_string_alloc();
        furi_string_printf(message, "\e# RENFE REGULAR\n\n");
        furi_string_cat_printf(message, " Live reading not supported\n\n");
        furi_string_cat_printf(message, "These cards use specific keys\n");
        furi_string_cat_printf(message, "that vary by card.\n\n");
        furi_string_cat_printf(message, " Load your dump from:\n");
        furi_string_cat_printf(message, "   Saved -> [filename].nfc\n\n");
        furi_string_cat_printf(message, " Supports all 18 regions:\n");
        furi_string_cat_printf(message, "   Valencia, Madrid, Catalunya,\n");
        furi_string_cat_printf(message, "   Andalucia, Pais Vasco,\n");
        furi_string_cat_printf(message, "   Galicia, and 12 more...\n\n");
        furi_string_cat_printf(message, " Use Proxmark3 or similar\n");
        furi_string_cat_printf(message, "   to dump the card first.");

        widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(message));
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

        furi_string_free(message);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    }
}

static bool renfe_regular_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card found!\nDon't move...", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card lost!\nTry again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Wrong card", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Read failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void renfe_regular_on_exit(Metroflip* app) {
    if(!app) return;
    widget_reset(app->widget);
    popup_reset(app->popup);
    metroflip_app_blink_stop(app);
    renfe_regular_clear_station_cache();
}

// ---------------------------------------------------------------------------
// Plugin descriptor
// ---------------------------------------------------------------------------

static const MetroflipPlugin renfe_regular_plugin = {
    .card_name = "RENFE Regular",
    .plugin_on_enter = renfe_regular_on_enter,
    .plugin_on_event = renfe_regular_on_event,
    .plugin_on_exit = renfe_regular_on_exit,
};

static const FlipperAppPluginDescriptor renfe_regular_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &renfe_regular_plugin,
};

const FlipperAppPluginDescriptor* renfe_regular_plugin_ep(void) {
    return &renfe_regular_plugin_descriptor;
}
