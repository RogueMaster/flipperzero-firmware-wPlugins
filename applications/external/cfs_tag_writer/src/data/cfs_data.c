#include "src/data/cfs_data.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <string.h>
#include <stdio.h>

// Byte offsets within the 48-byte plaintext payload.
#define CFS_OFF_DATE     (0)
#define CFS_OFF_VENDOR   (5)
#define CFS_OFF_BATCH    (9)
#define CFS_OFF_FILAMENT (11)
#define CFS_OFF_COLOR    (17)
#define CFS_OFF_LENGTH   (24)
#define CFS_OFF_SERIAL   (28)
#define CFS_OFF_RESERVED (34)

// Defined lower down; forward-declared so the catalog lookup above its
// definition can use it.
static size_t cfs_strnlen(const char* s, size_t max);

typedef struct {
    CfsFilamentType type;
    const char* id;
    const char* name;
} CfsFilamentEntry;

static const CfsFilamentEntry cfs_filament_table[] = {
    {CfsFilamentPLA, "101001", "PLA"},
    {CfsFilamentPETG, "101002", "PETG"},
    {CfsFilamentABS, "101003", "ABS"},
    {CfsFilamentTPU, "101004", "TPU"},
    {CfsFilamentNylon, "101005", "Nylon"},
    {CfsFilamentPLAAlt, "E00003", "PLA (alt)"},
    {CfsFilamentHyperPLA, "100003", "Hyper PLA"},
};
static const size_t cfs_filament_table_size =
    sizeof(cfs_filament_table) / sizeof(cfs_filament_table[0]);

const char* cfs_filament_type_to_name(CfsFilamentType type) {
    for(size_t i = 0; i < cfs_filament_table_size; i++) {
        if(cfs_filament_table[i].type == type) return cfs_filament_table[i].name;
    }
    return "Unknown";
}

CfsFilamentType cfs_filament_id_to_type(const char* id6) {
    for(size_t i = 0; i < cfs_filament_table_size; i++) {
        if(strncmp(cfs_filament_table[i].id, id6, CFS_FILAMENT_LEN) == 0) {
            return cfs_filament_table[i].type;
        }
    }
    return CfsFilamentUnknown;
}

// Authoritative catalog transcribed from Creality's material database (part
// of the various reference material described in CLAUDE.md, found online and
// not shipped in this repo). Keyed by the 5-char catalog id.
static const CfsFilamentInfo cfs_catalog[] = {
    {"00001", CfsBrandGeneric, "PLA", "Generic PLA", 190, 240},
    {"00002", CfsBrandGeneric, "PLA", "Generic PLA-Silk", 190, 240},
    {"00003", CfsBrandGeneric, "PETG", "Generic PETG", 220, 270},
    {"00004", CfsBrandGeneric, "ABS", "Generic ABS", 240, 280},
    {"00005", CfsBrandGeneric, "TPU", "Generic TPU", 210, 240},
    {"00006", CfsBrandGeneric, "PLA-CF", "Generic PLA-CF", 190, 240},
    {"00007", CfsBrandGeneric, "ASA", "Generic ASA", 240, 280},
    {"00008", CfsBrandGeneric, "PA", "Generic PA", 240, 260},
    {"00009", CfsBrandGeneric, "PA-CF", "Generic PA-CF", 260, 300},
    {"00010", CfsBrandGeneric, "BVOH", "Generic BVOH", 200, 220},
    {"00011", CfsBrandGeneric, "PVA", "Generic PVA", 215, 225},
    {"00012", CfsBrandGeneric, "HIPS", "Generic HIPS", 220, 250},
    {"00013", CfsBrandGeneric, "PET-CF", "Generic PET-CF", 280, 320},
    {"00014", CfsBrandGeneric, "PETG-CF", "Generic PETG-CF", 240, 260},
    {"00015", CfsBrandGeneric, "PA6-CF", "Generic PA6-CF", 280, 300},
    {"00016", CfsBrandGeneric, "PAHT-CF", "Generic PAHT-CF", 300, 320},
    {"00017", CfsBrandGeneric, "PPS", "Generic PPS", 320, 350},
    {"00018", CfsBrandGeneric, "PPS-CF", "Generic PPS-CF", 300, 350},
    {"00019", CfsBrandGeneric, "PP", "Generic PP", 220, 250},
    {"00020", CfsBrandGeneric, "PET", "Generic PET", 210, 230},
    {"00021", CfsBrandGeneric, "PC", "Generic PC", 250, 270},
    {"00022", CfsBrandGeneric, "PA-CF", "Generic PA612-CF", 270, 310},
    {"00023", CfsBrandGeneric, "PA", "Generic Support for PA", 260, 300},
    {"00024", CfsBrandGeneric, "PLA", "Generic Support for PLA", 190, 240},
    {"00025", CfsBrandGeneric, "PA-CF", "Generic PA12-CF", 270, 310},
    {"00026", CfsBrandGeneric, "TPU", "Generic TPU 64D", 210, 240},
    {"00027", CfsBrandGeneric, "PETG-GF", "Generic PETG-GF", 240, 280},
    {"00031", CfsBrandGeneric, "PP-CF", "Generic PP-CF", 220, 270},
    {"00032", CfsBrandGeneric, "PCTG", "Generic PCTG", 250, 270},
    {"00033", CfsBrandGeneric, "ASA-CF", "Generic ASA-CF", 250, 280},
    {"00034", CfsBrandGeneric, "PA-GF", "Generic PA6-GF", 250, 290},
    {"00035", CfsBrandESUN, "PLA", "PLA-LW", 190, 270},
    {"01001", CfsBrandCreality, "PLA", "Hyper PLA", 190, 240},
    {"01601", CfsBrandCreality, "PLA", "Soleyin Ultra PLA", 190, 240},
    {"02001", CfsBrandCreality, "PLA-CF", "Hyper PLA-CF", 190, 240},
    {"03001", CfsBrandCreality, "ABS", "Hyper ABS", 240, 280},
    {"04001", CfsBrandCreality, "PLA", "CR-PLA", 190, 240},
    {"05001", CfsBrandCreality, "PLA", "CR-Silk", 190, 240},
    {"06001", CfsBrandCreality, "PETG", "CR-PETG", 220, 270},
    {"06002", CfsBrandCreality, "PETG", "Hyper PETG", 220, 270},
    {"07001", CfsBrandCreality, "ABS", "CR-ABS", 240, 280},
    {"07002", CfsBrandCreality, "PC", "Hyper PC", 250, 270},
    {"08001", CfsBrandCreality, "PLA", "Ender-PLA", 190, 240},
    {"09001", CfsBrandCreality, "PLA", "EN-PLA+", 190, 240},
    {"09002", CfsBrandCreality, "PLA", "ENDER FAST PLA", 190, 240},
    {"10001", CfsBrandCreality, "TPU", "HP-TPU", 190, 240},
    {"11001", CfsBrandCreality, "PA", "CR-Nylon", 250, 270},
    {"12002", CfsBrandCreality, "PA-CF", "Hyper PPA-CF", 280, 320},
    {"12003", CfsBrandCreality, "PA-CF", "Hyper PAHT-CF", 280, 320},
    {"13001", CfsBrandCreality, "PLA", "CR-PLA Carbon", 190, 240},
    {"14001", CfsBrandCreality, "PLA", "CR-PLA Matte", 190, 240},
    {"15001", CfsBrandCreality, "PLA", "CR-PLA Fluo", 190, 240},
    {"16001", CfsBrandCreality, "TPU", "CR-TPU", 210, 240},
    {"17001", CfsBrandCreality, "PLA", "CR-Wood", 190, 240},
    {"18001", CfsBrandCreality, "PLA", "HP Ultra PLA", 190, 240},
    {"19001", CfsBrandCreality, "ASA", "HP-ASA", 240, 280},
};
static const size_t cfs_catalog_size = sizeof(cfs_catalog) / sizeof(cfs_catalog[0]);

size_t cfs_catalog_count(void) {
    return cfs_catalog_size;
}

const CfsFilamentInfo* cfs_catalog_entry(size_t index) {
    return (index < cfs_catalog_size) ? &cfs_catalog[index] : NULL;
}

const CfsFilamentInfo* cfs_filament_lookup(const char* filament_id6) {
    if(!filament_id6) return NULL;
    // A tag's 6-char id is a 1-char prefix + the 5-char catalog id; match on the
    // trailing 5 chars. Only the prefix "1" has been confirmed against physical
    // tags so far; other prefix values are unverified.
    size_t len = cfs_strnlen(filament_id6, CFS_FILAMENT_LEN);
    if(len < 5) return NULL;
    const char* tail = filament_id6 + (len - 5);
    for(size_t i = 0; i < cfs_catalog_size; i++) {
        if(strncmp(cfs_catalog[i].base_id, tail, 5) == 0) return &cfs_catalog[i];
    }
    return NULL;
}

static bool cfs_is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static bool cfs_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static uint8_t cfs_hex_val(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static size_t cfs_strnlen(const char* s, size_t max) {
    size_t n = 0;
    while(n < max && s[n] != '\0') n++;
    return n;
}

void cfs_data_set_defaults(CfsTagData* data) {
    furi_assert(data);
    memset(data, 0, sizeof(CfsTagData));
    // Date field: Creality's reference firmware writes the fixed header "AB124"
    // for all new tags (it is not a real calendar date). Clone/edit overwrites
    // this with the source tag's preserved value.
    strncpy(data->date_raw, "AB124", sizeof(data->date_raw));
    strncpy(data->vendor, "0276", sizeof(data->vendor)); // Creality
    strncpy(data->batch, "A2", sizeof(data->batch)); // literal, per K2-RFID source
    strncpy(data->filament_id, "101001", sizeof(data->filament_id));
    data->filament_type = CfsFilamentPLA;
    data->color_r = 0xFF;
    data->color_g = 0xFF;
    data->color_b = 0xFF; // white
    data->length_m = 330; // ~1kg PLA
    strncpy(data->serial, "000001", sizeof(data->serial));
}

bool cfs_data_decode(const uint8_t plaintext[CFS_PLAINTEXT_SIZE], CfsTagData* out) {
    furi_assert(plaintext);
    furi_assert(out);
    memset(out, 0, sizeof(CfsTagData));

    const char* p = (const char*)plaintext;

    memcpy(out->date_raw, p + CFS_OFF_DATE, CFS_DATE_LEN);
    memcpy(out->vendor, p + CFS_OFF_VENDOR, CFS_VENDOR_LEN);
    memcpy(out->batch, p + CFS_OFF_BATCH, CFS_BATCH_LEN);
    memcpy(out->filament_id, p + CFS_OFF_FILAMENT, CFS_FILAMENT_LEN);
    memcpy(out->serial, p + CFS_OFF_SERIAL, CFS_SERIAL_LEN);

    // Validate the fields whose charset we can rely on.
    for(int i = 0; i < CFS_COLOR_LEN; i++) {
        if(!cfs_is_hex_digit(p[CFS_OFF_COLOR + i])) return false;
    }
    for(int i = 0; i < CFS_LENGTH_LEN; i++) {
        if(!cfs_is_digit(p[CFS_OFF_LENGTH + i])) return false;
    }
    for(int i = 0; i < CFS_SERIAL_LEN; i++) {
        if(!cfs_is_digit(out->serial[i])) return false;
    }

    // Color is "0RRGGBB": skip leading nibble, decode 6 hex digits.
    const char* c = p + CFS_OFF_COLOR + 1;
    out->color_r = (cfs_hex_val(c[0]) << 4) | cfs_hex_val(c[1]);
    out->color_g = (cfs_hex_val(c[2]) << 4) | cfs_hex_val(c[3]);
    out->color_b = (cfs_hex_val(c[4]) << 4) | cfs_hex_val(c[5]);

    // Length is a 4-digit DECIMAL string of meters. (The spec doc's "hex"
    // claim is wrong: a real 1kg PLA tag reads "0330" == 330 m, matching the
    // physical ~330 m/kg; hex would give a nonsensical 816.)
    uint16_t len = 0;
    for(int i = 0; i < CFS_LENGTH_LEN; i++) {
        len = (len * 10) + (p[CFS_OFF_LENGTH + i] - '0');
    }
    out->length_m = len;

    out->filament_type = cfs_filament_id_to_type(out->filament_id);

    return true;
}

void cfs_data_encode(const CfsTagData* data, uint8_t plaintext[CFS_PLAINTEXT_SIZE]) {
    furi_assert(data);
    furi_assert(plaintext);
    memset(plaintext, 0, CFS_PLAINTEXT_SIZE);

    char* p = (char*)plaintext;
    char buf[16];

    // Date: written verbatim from date_raw (the fixed "AB124" header for new
    // tags, or the preserved source value on clone/edit). Pad with '0' if the
    // stored value is somehow short.
    memset(p + CFS_OFF_DATE, '0', CFS_DATE_LEN);
    size_t date_len = cfs_strnlen(data->date_raw, CFS_DATE_LEN);
    memcpy(p + CFS_OFF_DATE, data->date_raw, date_len);

    memset(p + CFS_OFF_VENDOR, '0', CFS_VENDOR_LEN);
    memcpy(p + CFS_OFF_VENDOR, data->vendor, cfs_strnlen(data->vendor, CFS_VENDOR_LEN));

    memset(p + CFS_OFF_BATCH, '0', CFS_BATCH_LEN);
    memcpy(p + CFS_OFF_BATCH, data->batch, cfs_strnlen(data->batch, CFS_BATCH_LEN));

    // Filament id: written verbatim. The write wizard sets this from the
    // authoritative catalog ("1" + brand-specific base_id), so encoding must not
    // second-guess it via the brand-blind legacy type table.
    memcpy(p + CFS_OFF_FILAMENT, data->filament_id, CFS_FILAMENT_LEN);

    // Color: "0RRGGBB".
    snprintf(
        buf,
        sizeof(buf),
        "0%02X%02X%02X",
        data->color_r,
        data->color_g,
        data->color_b);
    memcpy(p + CFS_OFF_COLOR, buf, CFS_COLOR_LEN);

    // Length: 4-digit DECIMAL meters (must match cfs_data_decode).
    snprintf(buf, sizeof(buf), "%04u", (data->length_m > 9999) ? 9999 : data->length_m);
    memcpy(p + CFS_OFF_LENGTH, buf, CFS_LENGTH_LEN);

    // Serial: 6 digits, zero-padded.
    snprintf(buf, sizeof(buf), "%.6s", data->serial);
    size_t slen = cfs_strnlen(buf, CFS_SERIAL_LEN);
    memset(p + CFS_OFF_SERIAL, '0', CFS_SERIAL_LEN);
    if(slen > 0) memcpy(p + CFS_OFF_SERIAL + (CFS_SERIAL_LEN - slen), buf, slen);

    // Reserved (34-47): real tags store ASCII '0' (0x30) here, not null bytes.
    memset(p + CFS_OFF_RESERVED, '0', CFS_RESERVED_LEN);
}

bool cfs_data_is_blank(const uint8_t payload[CFS_PLAINTEXT_SIZE]) {
    for(int i = 0; i < CFS_PLAINTEXT_SIZE; i++) {
        if(payload[i] != 0x00) return false;
    }
    return true;
}

// ---- Brand / vendor ----

const char* cfs_brand_to_vendor(CfsBrand brand) {
    switch(brand) {
    case CfsBrandCreality:
        return "0276";
    case CfsBrandGeneric:
    default:
        return "0000";
    }
}

const char* cfs_brand_to_name(CfsBrand brand) {
    switch(brand) {
    case CfsBrandCreality:
        return "Creality";
    case CfsBrandESUN:
        return "eSUN";
    case CfsBrandGeneric:
    default:
        return "Generic";
    }
}

CfsBrand cfs_vendor_to_brand(const char* vendor4) {
    if(vendor4 && strncmp(vendor4, "0276", CFS_VENDOR_LEN) == 0) return CfsBrandCreality;
    return CfsBrandGeneric;
}

const char* cfs_resolve_brand_name(const CfsTagData* data) {
    const CfsFilamentInfo* info = cfs_filament_lookup(data->filament_id);
    return info ? cfs_brand_to_name(info->brand) :
                  cfs_brand_to_name(cfs_vendor_to_brand(data->vendor));
}

// ---- Weight <-> length ----

const uint16_t cfs_weight_grams[CFS_WEIGHT_COUNT] = {250, 500, 600, 750, 1000};
const uint16_t cfs_default_weight_length[CFS_WEIGHT_COUNT] = {82, 165, 198, 247, 330};

const char* cfs_weight_label(size_t index) {
    static const char* const labels[CFS_WEIGHT_COUNT] =
        {"250 g", "500 g", "600 g", "750 g", "1 kg"};
    if(index >= CFS_WEIGHT_COUNT) return "?";
    return labels[index];
}

size_t cfs_nearest_weight_index(const uint16_t* len_table, uint16_t length_m) {
    size_t best = 0;
    uint16_t best_diff = 0xFFFF;
    for(size_t i = 0; i < CFS_WEIGHT_COUNT; i++) {
        uint16_t d = (len_table[i] > length_m) ? (len_table[i] - length_m) :
                                                 (length_m - len_table[i]);
        if(d < best_diff) {
            best_diff = d;
            best = i;
        }
    }
    return best;
}

// ---- Misc helpers ----

void cfs_serial_random(char out[CFS_SERIAL_LEN + 1]) {
    uint32_t r = furi_hal_random_get() % 1000000UL;
    snprintf(out, CFS_SERIAL_LEN + 1, "%06lu", (unsigned long)r);
}

void cfs_format_uid_hex(const uint8_t* uid, size_t uid_len, char* out, size_t out_size) {
    size_t pos = 0;
    for(size_t i = 0; i < uid_len && pos + 2 < out_size; i++) {
        pos += snprintf(out + pos, out_size - pos, "%02X", uid[i]);
    }
    if(pos == 0 && out_size > 0) out[0] = '\0';
}

void cfs_brand_material_label(const CfsTagData* data, char* out, size_t out_size) {
    // Prefer the authoritative catalog (brand + product name); fall back to the
    // vendor code / legacy type / raw id when the id isn't in the catalog.
    const CfsFilamentInfo* info = cfs_filament_lookup(data->filament_id);
    const char* brand = cfs_resolve_brand_name(data);
    const char* material = info ? info->name :
                           (data->filament_type != CfsFilamentUnknown) ?
                                                cfs_filament_type_to_name(data->filament_type) :
                                                data->filament_id;
    // Generic catalog names already embed the brand ("Generic ABS"); skip the
    // brand prefix when it would double up ("Generic Generic ABS").
    if(strncmp(material, brand, strlen(brand)) == 0) {
        snprintf(out, out_size, "%s", material);
    } else {
        snprintf(out, out_size, "%s %s", brand, material);
    }
}

void cfs_build_save_name(const CfsTagData* data, char* out, size_t out_size) {
    char label[48];
    cfs_brand_material_label(data, label, sizeof(label));
    // Include the serial so distinct spools get distinct default names (two
    // spools with the same brand/material/color would otherwise collide).
    snprintf(
        out,
        out_size,
        "%s %02X%02X%02X %s",
        label,
        data->color_r,
        data->color_g,
        data->color_b,
        data->serial);
}

// 32 named colors. Basic = first 8, Standard = first 16, Extended = all 32.
const CfsColorPreset cfs_color_table[CFS_COLOR_TABLE_MAX] = {
    // Basic (0-7)
    {"Black",        0x00, 0x00, 0x00},
    {"White",        0xFF, 0xFF, 0xFF},
    {"Gray",         0x80, 0x80, 0x80},
    {"Red",          0xFF, 0x00, 0x00},
    {"Green",        0x00, 0x80, 0x00},
    {"Blue",         0x00, 0x00, 0xFF},
    {"Yellow",       0xFF, 0xFF, 0x00},
    {"Orange",       0xFF, 0xA5, 0x00},

    // Standard (8-15)
    {"Silver",       0xC0, 0xC0, 0xC0},
    {"Dark Gray",    0x40, 0x40, 0x40},
    {"Brown",        0x8B, 0x45, 0x13},
    {"Purple",       0x80, 0x00, 0x80},
    {"Pink",         0xFF, 0xC0, 0xCB},
    {"Cyan",         0x00, 0xFF, 0xFF},
    {"Gold",         0xFF, 0xD7, 0x00},
    {"Clear",        0xF8, 0xF8, 0xFF},

    // Extended (16-31)
    {"Navy",         0x00, 0x00, 0x80},
    {"Teal",         0x00, 0x80, 0x80},
    {"Sky Blue",     0x87, 0xCE, 0xEB},
    {"Forest Green", 0x22, 0x8B, 0x22},
    {"Olive",        0x80, 0x80, 0x00},
    {"Lime",         0x32, 0xCD, 0x32},
    {"Magenta",      0xFF, 0x00, 0xFF},
    {"Violet",       0xEE, 0x82, 0xEE},
    {"Beige",        0xF5, 0xF5, 0xDC},
    {"Tan",          0xD2, 0xB4, 0x8C},
    {"Copper",       0xB8, 0x73, 0x33},
    {"Khaki",        0xF0, 0xE6, 0x8C},
    {"Mint",         0x98, 0xFF, 0x98},
    {"Lavender",     0xE6, 0xE6, 0xFA},
    {"Maroon",       0x80, 0x00, 0x00},
    {"Turquoise",    0x40, 0xE0, 0xD0},
};

size_t cfs_palette_count(CfsColorPalette palette) {
    switch(palette) {
    case CfsPaletteStandard16:
        return 16;
    case CfsPaletteExtended32:
        return 32;
    case CfsPaletteBasic:
    default:
        return 8;
    }
}

const char* cfs_palette_label(CfsColorPalette palette) {
    switch(palette) {
    case CfsPaletteStandard16:
        return "16 std";
    case CfsPaletteExtended32:
        return "32 ext";
    case CfsPaletteBasic:
    default:
        return "Basic 8";
    }
}
