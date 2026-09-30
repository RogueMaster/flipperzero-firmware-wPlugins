#include "k2_database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const K2Material K2_MATERIALS[] = {
    {"01001", "Creality", "Hyper PLA", "PLA", "FFFFFF", 220, 50, 7},
    {"02001", "Creality", "Hyper PLA-CF", "PLA-CF", "FFFFFF", 220, 50, 7},
    {"06002", "Creality", "Hyper PETG", "PETG", "000000", 250, 70, 7},
    {"03001", "Creality", "Hyper ABS", "ABS", "FFFFFF", 260, 80, 7},
    {"09002", "Creality", "ENDER FAST PLA", "PLA", "000000", 220, 50, 5},
    {"04001", "Creality", "CR-PLA", "PLA", "FFFFFF", 220, 50, 7},
    {"05001", "Creality", "CR-Silk", "PLA", "000000", 220, 50, 7},
    {"06001", "Creality", "CR-PETG", "PETG", "FFFFFF", 240, 70, 7},
    {"07001", "Creality", "CR-ABS", "ABS", "FFFFFF", 260, 100, 7},
    {"00001", "Generic", "Generic PLA", "PLA", "FFFFFF", 220, 50, 7},
    {"00002", "Generic", "Generic PLA-Silk", "PLA", "FFFFFF", 220, 50, 7},
    {"00003", "Generic", "Generic PETG", "PETG", "FFFFFF", 250, 70, 7},
    {"00004", "Generic", "Generic ABS", "ABS", "FFFFFF", 260, 100, 7},
    {"00005", "Generic", "Generic TPU", "TPU", "FFFFFF", 220, 40, 3},
    {"00006", "Generic", "Generic PLA-CF", "PLA-CF", "FFFFFF", 220, 50, 3},
    {"00007", "Generic", "Generic ASA", "ASA", "FFFFFF", 260, 90, 3},
    {"08001", "Creality", "Ender-PLA", "PLA", "FFFFFF", 220, 50, 7},
    {"09001", "Creality", "EN-PLA+", "PLA", "FFFFFF", 220, 50, 3},
    {"10001", "Creality", "HP-TPU", "TPU", "FFFFFF", 220, 40, 7},
    {"11001", "Creality", "CR-Nylon", "PA", "FFFFFF", 260, 50, 3},
    {"13001", "Creality", "CR-PLA Carbon", "PLA-CF", "FFFFFF", 220, 50, 3},
    {"14001", "Creality", "CR-PLA Matte", "PLA", "FFFFFF", 220, 50, 3},
    {"15001", "Creality", "CR-PLA Fluo", "PLA", "FFFFFF", 220, 50, 3},
    {"16001", "Creality", "CR-TPU", "TPU", "FFFFFF", 220, 40, 3},
    {"17001", "Creality", "CR-Wood", "PLA", "FFFFFF", 220, 50, 3},
    {"18001", "Creality", "HP Ultra PLA", "PLA", "FFFFFF", 220, 50, 3},
    {"19001", "Creality", "HP-ASA", "ASA", "FFFFFF", 260, 90, 3},
    {"00008", "Generic", "Generic PA", "PA", "FFFFFF", 260, 50, 3},
    {"00009", "Generic", "Generic PA-CF", "PA-CF", "FFFFFF", 280, 60, 3},
    {"00010", "Generic", "Generic BVOH", "BVOH", "FFFFFF", 220, 50, 7},
    {"00011", "Generic", "Generic PVA", "PVA", "FFFFFF", 220, 50, 7},
    {"00012", "Generic", "Generic HIPS", "HIPS", "FFFFFF", 240, 100, 3},
    {"00013", "Generic", "Generic PET-CF", "PET-CF", "FFFFFF", 290, 90, 3},
    {"00014", "Generic", "Generic PETG-CF", "PETG-CF", "FFFFFF", 250, 80, 3},
    {"00015", "Generic", "Generic PA6-CF", "PA6-CF", "FFFFFF", 300, 60, 3},
    {"00016", "Generic", "Generic PAHT-CF", "PAHT-CF", "FFFFFF", 300, 90, 3},
    {"00017", "Generic", "Generic PPS", "PPS", "FFFFFF", 310, 90, 3},
    {"00018", "Generic", "Generic PPS-CF", "PPS-CF", "FFFFFF", 310, 105, 3},
    {"00019", "Generic", "Generic PP", "PP", "FFFFFF", 240, 40, 3},
    {"00020", "Generic", "Generic PET", "PET", "FFFFFF", 260, 90, 3},
    {"00021", "Generic", "Generic PC", "PC", "FFFFFF", 260, 110, 3},
    {"00025", "Generic", "Generic PA12-CF", "PA-CF", "000000", 290, 60, 1},
    {"00022", "Generic", "Generic PA612-CF", "PA-CF", "000000", 290, 60, 1},
    {"12003", "Creality", "Hyper PAHT-CF", "PA-CF", "000000", 290, 90, 3},
    {"12002", "Creality", "Hyper PPA-CF", "PA-CF", "000000", 300, 100, 1},
    {"00023", "Generic", "Generic Support for PA", "PA", "000000", 290, 80, 1},
    {"00024", "Generic", "Generic Support for PLA", "PLA", "000000", 220, 50, 1},
    {"00026", "Generic", "Generic TPU 64D", "TPU", "000000", 220, 40, 1},
    {"07002", "Creality", "Hyper PC", "PC", "000000", 260, 110, 1},
    {"01601", "Creality", "Soleyin Ultra PLA", "PLA", "000000", 220, 50, 7},
    {"00033", "Generic", "Generic ASA-CF", "ASA-CF", "000000", 270, 90, 1},
    {"00034", "Generic", "Generic PA6-GF", "PA-GF", "000000", 270, 100, 1},
    {"00035", "eSUN", "PLA-LW", "PLA", "000000", 220, 50, 1},
    {"00027", "Generic", "Generic PETG-GF", "PETG-GF", "000000", 270, 70, 1},
    {"00031", "Generic", "Generic PP-CF", "PP-CF", "000000", 260, 80, 1},
    {"00032", "Generic", "Generic PCTG", "PCTG", "000000", 250, 70, 1},
    {"06003", "Creality", "Hyper PETG-CF", "PETG-CF", "000000", 250, 70, 3},
    {"01002", "Creality", "Hyper L-W PLA", "PLA", "FFFFFF", 220, 50, 7},
    {"01004", "Creality", "Hyper Stardust", "PLA", "000000", 220, 50, 7},
    {"29001", "Creality", "Hyper Marble", "PLA", "000000", 220, 50, 7},
    {"12004", "Creality", "Hyper PA612-CF", "PA612-CF", "000000", 290, 60, 1},
    {"12005", "Creality", "Hyper PA6-CF", "PA6-CF", "000000", 300, 60, 1},
    {"E1001", "eSUN", "PLA+", "PLA", "000000", 220, 55, 1},
    {"P1001", "Polymaker", "Panchroma PLA Satin", "PLA", "000000", 230, 50, 1},
    {"P1002", "Polymaker", "PolySonic PLA Pro", "PLA", "000000", 220, 50, 1},
    {"P1003", "Polymaker", "Panchroma PLA Matte", "PLA", "000000", 210, 50, 1},
};

static const K2ColorPreset K2_COLORS[] = {
    {"White", "FFFFFF", 0xFF, 0xFF, 0xFF},
    {"Black", "000000", 0x00, 0x00, 0x00},
    {"Gray", "808080", 0x80, 0x80, 0x80},
    {"Silver", "C0C0C0", 0xC0, 0xC0, 0xC0},
    {"Red", "E02020", 0xE0, 0x20, 0x20},
    {"Crimson", "C12E1F", 0xC1, 0x2E, 0x1F},
    {"Blue", "0055FF", 0x00, 0x55, 0xFF},
    {"Navy", "000080", 0x00, 0x00, 0x80},
    {"Light Blue", "00BFFF", 0x00, 0xBF, 0xFF},
    {"Green", "10B010", 0x10, 0xB0, 0x10},
    {"Lime Green", "32CD32", 0x32, 0xCD, 0x32},
    {"Yellow", "FFDF00", 0xFF, 0xDF, 0x00},
    {"Orange", "FF8800", 0xFF, 0x88, 0x00},
    {"Purple", "800080", 0x80, 0x00, 0x80},
    {"Pink", "FF69B4", 0xFF, 0x69, 0xB4},
    {"Brown", "8B4513", 0x8B, 0x45, 0x13},
    {"Gold", "D4AF37", 0xD4, 0xAF, 0x37},
    {"Clear/Natural", "F0F8FF", 0xF0, 0xF8, 0xFF},
};

static const K2WeightOption K2_WEIGHTS[] = {
    {"1 KG", "0330", 330, 1000},
    {"750 G", "0247", 247, 750},
    {"600 G", "0198", 198, 600},
    {"500 G", "0165", 165, 500},
    {"250 G", "0082", 82, 250},
};

static const char* K2_PRINTERS[] = {
    "K2",
    "K1",
    "HI",
};

size_t k2_db_get_material_count(void) {
    return sizeof(K2_MATERIALS) / sizeof(K2_MATERIALS[0]);
}

const K2Material* k2_db_get_material(size_t index) {
    if (index >= k2_db_get_material_count()) return NULL;
    return &K2_MATERIALS[index];
}

const K2Material* k2_db_find_material_by_id(const char* id) {
    if (!id) return NULL;
    size_t count = k2_db_get_material_count();
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(K2_MATERIALS[i].id, id) == 0) {
            return &K2_MATERIALS[i];
        }
    }
    return NULL;
}

size_t k2_db_get_color_count(void) {
    return sizeof(K2_COLORS) / sizeof(K2_COLORS[0]);
}

const K2ColorPreset* k2_db_get_color(size_t index) {
    if (index >= k2_db_get_color_count()) return NULL;
    return &K2_COLORS[index];
}

static uint8_t hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

const char* k2_db_find_closest_color_name(const char* hex_str) {
    if (!hex_str || strlen(hex_str) < 6) return "White";
    /* Skip leading '#' or '0' if present */
    const char* p = hex_str;
    if (*p == '#') p++;
    if (strlen(p) == 7 && *p == '0') p++; /* handle "0FFFFFF" */
    if (strlen(p) < 6) return "White";

    uint8_t r = (hex_val(p[0]) << 4) | hex_val(p[1]);
    uint8_t g = (hex_val(p[2]) << 4) | hex_val(p[3]);
    uint8_t b = (hex_val(p[4]) << 4) | hex_val(p[5]);

    uint32_t min_dist_sq = 0xFFFFFFFF;
    const char* closest_name = "White";
    size_t count = k2_db_get_color_count();

    for (size_t i = 0; i < count; i++) {
        int32_t dr = (int32_t)r - (int32_t)K2_COLORS[i].r;
        int32_t dg = (int32_t)g - (int32_t)K2_COLORS[i].g;
        int32_t db = (int32_t)b - (int32_t)K2_COLORS[i].b;
        uint32_t dist_sq = (uint32_t)(dr * dr + dg * dg + db * db);
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            closest_name = K2_COLORS[i].name;
        }
    }
    return closest_name ? closest_name : "White";
}

size_t k2_db_get_weight_count(void) {
    return sizeof(K2_WEIGHTS) / sizeof(K2_WEIGHTS[0]);
}

const K2WeightOption* k2_db_get_weight(size_t index) {
    if (index >= k2_db_get_weight_count()) return NULL;
    return &K2_WEIGHTS[index];
}

const char* k2_db_find_weight_label_by_code(const char* code) {
    if (!code || strlen(code) < 4) return "1 KG";
    size_t count = k2_db_get_weight_count();
    for (size_t i = 0; i < count; i++) {
        if (strncmp(K2_WEIGHTS[i].code, code, 4) == 0) {
            return K2_WEIGHTS[i].label;
        }
    }
    return "1 KG";
}

size_t k2_db_get_printer_count(void) {
    return sizeof(K2_PRINTERS) / sizeof(K2_PRINTERS[0]);
}

const char* k2_db_get_printer_name(size_t index) {
    if (index >= k2_db_get_printer_count()) return NULL;
    return K2_PRINTERS[index];
}

bool k2_build_payload(const K2SpoolConfig* config, uint8_t sector1_out[48], uint8_t sector2_out[48]) {
    if (!config || !sector1_out || !sector2_out) return false;

    const char* date = config->date[0] ? config->date : "AB124";
    const char* vendor = config->vendor_id[0] ? config->vendor_id : "0276";
    const char* batch = config->batch[0] ? config->batch : "A2";
    const char* mat_id = config->material_id[0] ? config->material_id : "01001";
    const char* color = config->color_hex[0] ? config->color_hex : "FFFFFF";
    const char* weight = config->weight_code[0] ? config->weight_code : "0330";
    const char* serial = config->serial[0] ? config->serial : "000001";
    const char* printer = config->printer_model[0] ? config->printer_model : "K2";

    /* Format Sector 1 payload (48 bytes):
     * [0..4]: Date (5 chars)
     * [5..8]: Vendor (4 chars)
     * [9..10]: Batch (2 chars)
     * [11..16]: Filament ID: '1' + 5 chars mat_id (6 chars)
     * [17..23]: Color: '0' + 6 chars color (7 chars)
     * [24..27]: Length code (4 chars)
     * [28..33]: Serial (6 chars)
     * [34..47]: Reserved ("00000000000000", 14 chars)
     * Total = 48 chars
     */
    char s1_buf[64];
    snprintf(s1_buf, sizeof(s1_buf),
             "%-5.5s%-4.4s%-2.2s1%-5.5s0%-6.6s%-4.4s%-6.6s00000000000000",
             date, vendor, batch, mat_id, color, weight, serial);

    memcpy(sector1_out, s1_buf, 48);

    /* Format Sector 2 payload (48 bytes):
     * Printer Model string padded with spaces to 48 bytes
     */
    memset(sector2_out, ' ', 48);
    size_t pr_len = strlen(printer);
    if (pr_len > 48) pr_len = 48;
    memcpy(sector2_out, printer, pr_len);

    return true;
}

bool k2_parse_payload(const uint8_t sector1[48], const uint8_t sector2[48], K2SpoolInfo* info_out) {
    if (!sector1 || !sector2 || !info_out) return false;

    memset(info_out, 0, sizeof(K2SpoolInfo));
    memcpy(info_out->raw_sector1, sector1, 48);
    memcpy(info_out->raw_sector2, sector2, 48);

    /* Verify basic ASCII validity */
    for (size_t i = 0; i < 34; i++) {
        if (sector1[i] < 0x20 || sector1[i] > 0x7E) {
            info_out->valid = false;
            return false;
        }
    }

    /* Extract fields according to CFS specification */
    memcpy(info_out->date, sector1 + 0, 5);
    info_out->date[5] = '\0';

    memcpy(info_out->vendor_id, sector1 + 5, 4);
    info_out->vendor_id[4] = '\0';

    memcpy(info_out->batch, sector1 + 9, 2);
    info_out->batch[2] = '\0';

    /* Filament ID is at [11..16], where [11] is '1' and [12..16] is material_id */
    memcpy(info_out->material_id, sector1 + 12, 5);
    info_out->material_id[5] = '\0';

    /* Color is at [17..23], where [17] is '0' and [18..23] is 6 hex characters */
    memcpy(info_out->color_hex, sector1 + 18, 6);
    info_out->color_hex[6] = '\0';

    /* Length code is at [24..27] (4 chars) */
    memcpy(info_out->length_code, sector1 + 24, 4);
    info_out->length_code[4] = '\0';

    /* Serial is at [28..33] (6 chars) */
    memcpy(info_out->serial, sector1 + 28, 6);
    info_out->serial[6] = '\0';

    /* Extract Printer Model from Sector 2 (blocks 8..10) */
    char pr_buf[49];
    memcpy(pr_buf, sector2, 48);
    pr_buf[48] = '\0';
    /* Trim trailing spaces */
    for (int i = 47; i >= 0; i--) {
        if (pr_buf[i] == ' ' || pr_buf[i] == '\0') {
            pr_buf[i] = '\0';
        } else {
            break;
        }
    }
    strncpy(info_out->printer_model, pr_buf, sizeof(info_out->printer_model) - 1);
    if (info_out->printer_model[0] == '\0') {
        strncpy(info_out->printer_model, "K2", sizeof(info_out->printer_model) - 1);
    }

    /* Look up material in database */
    info_out->material = k2_db_find_material_by_id(info_out->material_id);

    /* Look up color name and weight label */
    const char* c_name = k2_db_find_closest_color_name(info_out->color_hex);
    snprintf(info_out->color_name, sizeof(info_out->color_name), "%s", c_name ? c_name : "White");

    const char* w_label = k2_db_find_weight_label_by_code(info_out->length_code);
    snprintf(info_out->weight_label, sizeof(info_out->weight_label), "%s", w_label ? w_label : "1 KG");

    info_out->valid = true;
    return true;
}
