#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CFS_PLAINTEXT_SIZE (48)

// Field lengths per the CFS tag byte-offset table.
#define CFS_DATE_LEN     (5)
#define CFS_VENDOR_LEN   (4)
#define CFS_BATCH_LEN    (2)
#define CFS_FILAMENT_LEN (6)
#define CFS_COLOR_LEN    (7)
#define CFS_LENGTH_LEN   (4)
#define CFS_SERIAL_LEN   (6)
#define CFS_RESERVED_LEN (14)

typedef enum {
    CfsFilamentUnknown = 0,
    CfsFilamentPLA, // "101001"
    CfsFilamentPETG, // "101002"
    CfsFilamentABS, // "101003"
    CfsFilamentTPU, // "101004"
    CfsFilamentNylon, // "101005"
    CfsFilamentPLAAlt, // "E00003"
    CfsFilamentHyperPLA, // "100003" (confirmed from a real Hyper PLA spool)
    CfsFilamentTypeCount,
} CfsFilamentType;

typedef struct {
    char date_raw[CFS_DATE_LEN + 1]; // opaque passthrough (encoding not reversed)
    char vendor[CFS_VENDOR_LEN + 1]; // e.g. "0276"
    char batch[CFS_BATCH_LEN + 1]; // fixed default "A2" (see cfs_data_set_defaults)
    char filament_id[CFS_FILAMENT_LEN + 1]; // e.g. "101001"
    CfsFilamentType filament_type; // decoded from filament_id
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint16_t length_m; // decoded from a 4-digit decimal ASCII field
    char serial[CFS_SERIAL_LEN + 1]; // 6 digits
} CfsTagData;

/** Initialize with sane write-flow defaults (vendor 0276, batch 01, etc.). */
void cfs_data_set_defaults(CfsTagData* data);

/**
 * Decode a 48-byte plaintext payload into structured fields.
 * Returns false if fields fail charset validation (likely a decrypt failure or
 * a non-CFS tag), so callers can distinguish garbage from valid data.
 */
bool cfs_data_decode(const uint8_t plaintext[CFS_PLAINTEXT_SIZE], CfsTagData* out);

/** Encode structured fields into a 48-byte plaintext payload. */
void cfs_data_encode(const CfsTagData* data, uint8_t plaintext[CFS_PLAINTEXT_SIZE]);

/** True if all 48 bytes are 0x00 (a blank/unwritten tag). */
bool cfs_data_is_blank(const uint8_t payload[CFS_PLAINTEXT_SIZE]);

/** Human-readable material name, e.g. "PLA". */
const char* cfs_filament_type_to_name(CfsFilamentType type);

/** Map a 6-char material ID to an enum (CfsFilamentUnknown if unrecognized). */
CfsFilamentType cfs_filament_id_to_type(const char* id6);

// ---- Brand / vendor ----

typedef enum {
    CfsBrandGeneric, // vendor "0000"
    CfsBrandCreality, // vendor "0276"
    CfsBrandESUN, // catalog-only (no known 4-char vendor code)
    CfsBrandCount,
} CfsBrand;

// Brands offered in the write / settings pickers (each writes a vendor code).
// eSUN is read/catalog-only for now, so it's excluded from writing.
#define CFS_BRAND_WRITABLE_COUNT (2)

// ---- Authoritative filament catalog (from Creality's material database) ----
// Keyed by the 5-char catalog id; a tag's 6-char filament_id is matched by its
// trailing 5 chars (e.g. tag "101001" -> catalog "01001" = "Hyper PLA"). This is
// the source of truth for the read-side product name, brand, and material type.
typedef struct {
    const char* base_id; // 5-char catalog id
    CfsBrand brand;
    const char* material_type; // category, e.g. "PLA", "PETG-CF"
    const char* name; // product name, e.g. "Hyper PLA"
    uint16_t min_temp;
    uint16_t max_temp;
} CfsFilamentInfo;

/**
 * Look up a tag's 6-char filament_id in the catalog (matches the trailing 5
 * chars). Returns NULL if the id isn't in the catalog.
 */
const CfsFilamentInfo* cfs_filament_lookup(const char* filament_id6);

/** Number of entries in the authoritative filament catalog. */
size_t cfs_catalog_count(void);

/** Catalog entry by index, or NULL if out of range. */
const CfsFilamentInfo* cfs_catalog_entry(size_t index);

// ---- Date field ----
// 5 bytes at payload offset 0. This is an opaque Creality header, not a real
// calendar date: the reference firmware writes the constant "AB124" for new
// tags. Factory tags carry other values (e.g. "AB625") we don't reverse.

// ---- Brand helpers (CfsBrand is defined above, before the catalog) ----

/** 4-char vendor code for a brand, e.g. "0276". */
const char* cfs_brand_to_vendor(CfsBrand brand);

/** Human-readable brand name, e.g. "Creality". */
const char* cfs_brand_to_name(CfsBrand brand);

/** Map a 4-char vendor code to a brand (defaults to Generic if unknown). */
CfsBrand cfs_vendor_to_brand(const char* vendor4);

/**
 * Resolve a tag's display brand name: the authoritative catalog entry when
 * filament_id matches, else the vendor-code fallback. Shared by every screen
 * that shows a tag's brand.
 */
const char* cfs_resolve_brand_name(const CfsTagData* data);

// ---- Weight <-> length ----
// Fixed set of spool weights; the length for each is configurable (held in
// settings) so only the length values are passed in here. Defaults match real
// Creality tags (decimal meters).

#define CFS_WEIGHT_COUNT (5)

extern const uint16_t cfs_weight_grams[CFS_WEIGHT_COUNT]; // {250,500,600,750,1000}
extern const uint16_t cfs_default_weight_length[CFS_WEIGHT_COUNT]; // {82,165,198,247,330}

/** Human label for a weight index, e.g. "1 kg" or "500 g". */
const char* cfs_weight_label(size_t index);

/** Index of the weight whose length is closest to length_m (for display). */
size_t cfs_nearest_weight_index(const uint16_t* len_table, uint16_t length_m);

// ---- Color palette ----
// A single named-color table; the active palette selects how many of these are
// offered in the write-color picker. "Custom..." is appended by the UI.

typedef struct {
    const char* name;
    uint8_t r, g, b;
} CfsColorPreset;

#define CFS_COLOR_TABLE_MAX (32)

extern const CfsColorPreset cfs_color_table[CFS_COLOR_TABLE_MAX];

typedef enum {
    CfsPaletteBasic, // first 8 colors
    CfsPaletteStandard16, // first 16 colors
    CfsPaletteExtended32, // all 32 colors
    CfsPaletteCount,
} CfsColorPalette;

/** Number of colors offered for a palette (8 / 16 / 32). */
size_t cfs_palette_count(CfsColorPalette palette);

/** Human label for a palette, e.g. "16 std". */
const char* cfs_palette_label(CfsColorPalette palette);

// ---- Misc helpers ----

/** Fill out with 6 random ASCII digits + NUL. */
void cfs_serial_random(char out[CFS_SERIAL_LEN + 1]);

/** Format a UID as uppercase hex (e.g. "04A1B2C3"). Writes "" if uid_len == 0. */
void cfs_format_uid_hex(const uint8_t* uid, size_t uid_len, char* out, size_t out_size);

/**
 * Write a "brand material" display label for a tag, e.g. "Creality Hyper PLA" or
 * "Generic ABS". Catalog-driven (legacy-type / raw-id fallback); skips the brand
 * prefix when the catalog name already embeds it (avoids "Generic Generic ABS").
 */
void cfs_brand_material_label(const CfsTagData* data, char* out, size_t out_size);

/** Build a filename-safe "brand material color serial" label. */
void cfs_build_save_name(const CfsTagData* data, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif
