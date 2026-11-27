#pragma once
#include <storage/storage.h>
#include "companion_catalog.h"
#include "cheat_catalog.h"
#include "safe_codes.h"
#include "secret_missions.h"
#include "dispatch_catalog.h"
#include <stdbool.h>

/* UTF-8 TSV: one escaped record per line. Build temporary row offsets on
 * demand; keep only the requested row in memory. */
#define GTA_RECORD_MAX       4096u
#define GTA_RECORD_FIELDS    9u
#define GTA_CATALOG_MAX_SIZE (256u * 1024u)
#define GTA_CATALOG_MAX_ROWS 4096u
typedef struct {
    char* data;
    char* field[GTA_RECORD_FIELDS];
} GtaRecord;
void* gta_try_alloc(size_t size);
size_t gta_tsv_row_count(File* file);
void gta_record_clear(GtaRecord* record);
bool gta_record_read(
    Storage* storage,
    const char* asset,
    size_t index,
    size_t fields,
    GtaRecord* record);
size_t gta_record_count(Storage* storage, const char* asset);
bool gta_record_find(
    Storage* storage,
    const char* asset,
    const char* key,
    size_t fields,
    GtaRecord* record);
bool gta_record_decode(GtaRecord* record, size_t size, size_t fields, bool escaped);
void gta_record_index_release(void);
extern const char* const gta_vehicle_assets[GtaCollectionCount];
extern const size_t gta_dispatch_source_count;
const GtaCheat* gta_cheat_at(size_t index, bool detail, unsigned slot);
const GtaScreenshotCode* gta_screenshot_at(size_t index, bool detail);
const GtaSafeCode* gta_safe_at(size_t index, bool detail);
const GtaSecretMission* gta_secret_at(size_t index, bool detail);
void gta_catalog_release(void);
void gta_url_links_release(void);
void gta_ram_log(const char* stage);
bool gta_dispatch_read(File* file, uint32_t* offset, GtaDispatchTarget* target);
