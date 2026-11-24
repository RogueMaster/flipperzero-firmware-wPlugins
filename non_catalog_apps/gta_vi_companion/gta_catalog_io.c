#include "gta_catalog_io.h"
#include "secret_progress.h"
#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gta_record_clear(GtaRecord* r) {
    free(r->data);
    memset(r, 0, sizeof(*r));
}
/* One temporary offset table for the currently used guide catalog. No
 * generated sidecars, and no scanning or allocation until a guide opens. */
static uint32_t* row_offsets;
static size_t row_count;
static char indexed_asset[48];
void gta_record_index_release(void) {
    free(row_offsets);
    row_offsets = NULL;
    row_count = 0;
    indexed_asset[0] = 0;
}
static File* open_catalog(Storage* storage, const char* asset) {
    char path[96];
    size_t len = strlen(asset);
    if(len < 5 || len >= sizeof(indexed_asset) || strcmp(asset + len - 4, ".tsv")) return NULL;
    int n = snprintf(path, sizeof(path), APP_ASSETS_PATH("catalogs/%s"), asset);
    if(n < 0 || (size_t)n >= sizeof(path)) return NULL;
    File* file = storage_file_alloc(storage);
    if(file && !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return NULL;
    }
    return file;
}
static bool build_index(File* file, const char* asset) {
    uint64_t size = storage_file_size(file);
    if(row_offsets && !strcmp(indexed_asset, asset) && row_offsets[row_count] == size) return true;
    gta_record_index_release();
    if(!size || size > 256u * 1024u || !storage_file_seek(file, 0, true)) return false;
    uint8_t chunk[256];
    uint32_t pos = 0, start = 0;
    size_t capacity = 0;
    while(pos < size) {
        size_t n = size - pos > sizeof(chunk) ? sizeof(chunk) : (size_t)(size - pos);
        if(storage_file_read(file, chunk, n) != n) goto fail;
        for(size_t i = 0; i < n; ++i, ++pos) {
            if(!chunk[i] || pos - start >= GTA_RECORD_MAX) goto fail;
            if(chunk[i] != '\n') continue;
            if(pos == start || row_count >= 4096u) goto fail;
            if(row_count == capacity) {
                size_t next = capacity ? capacity * 2u : 16u;
                uint32_t* tmp = realloc(row_offsets, (next + 1u) * sizeof(*tmp));
                if(!tmp) goto fail;
                row_offsets = tmp;
                capacity = next;
            }
            row_offsets[row_count++] = start;
            start = pos + 1u;
        }
    }
    if(start != size || !row_count) goto fail;
    row_offsets[row_count] = start;
    uint32_t* exact = realloc(row_offsets, (row_count + 1u) * sizeof(*exact));
    if(exact) row_offsets = exact;
    snprintf(indexed_asset, sizeof(indexed_asset), "%s", asset);
    return true;
fail:
    gta_record_index_release();
    return false;
}
size_t gta_record_count(Storage* storage, const char* asset) {
    File* file = open_catalog(storage, asset);
    if(!file) return 0;
    size_t count = build_index(file, asset) ? row_count : 0;
    storage_file_free(file);
    return count;
}
/* Split tabs first, then decode field escapes in place for guide catalogs. */
bool gta_record_decode(GtaRecord* r, size_t size, size_t fields, bool escaped) {
    if(!r->data || !size || !fields || fields > GTA_RECORD_FIELDS || r->data[size - 1] != '\n' ||
       memchr(r->data, 0, size))
        return false;
    r->data[size - 1] = 0;
    size_t field = 0;
    r->field[field++] = r->data;
    for(size_t i = 0; i + 1 < size; ++i) {
        if(r->data[i] == '\t') {
            if(field >= fields) return false;
            r->data[i] = 0;
            r->field[field++] = r->data + i + 1;
        } else if(r->data[i] == '\n' || r->data[i] == '\r')
            return false;
    }
    if(field != fields) return false;
    if(!escaped) return true;
    for(size_t i = 0; i < fields; ++i) {
        char* src = r->field[i];
        char* dst = src;
        while(*src) {
            char ch = *src++;
            if(ch == '\\') {
                switch(*src) {
                case '\\':
                    ch = '\\';
                    break;
                case 't':
                    ch = '\t';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                default:
                    return false;
                }
                ++src;
            }
            *dst++ = ch;
        }
        *dst = 0;
    }
    return true;
}
bool gta_record_read(
    Storage* storage,
    const char* asset,
    size_t index,
    size_t fields,
    GtaRecord* r) {
    gta_record_clear(r);
    File* file = open_catalog(storage, asset);
    if(!file) return false;
    bool ok = false;
    if(build_index(file, asset) && index < row_count) {
        uint32_t start = row_offsets[index], end = row_offsets[index + 1u];
        size_t n = end - start;
        if(n && n <= GTA_RECORD_MAX) {
            r->data = malloc(n);
            ok = r->data && storage_file_seek(file, start, true) &&
                 storage_file_read(file, r->data, n) == n && gta_record_decode(r, n, fields, true);
        }
    }
    storage_file_free(file);
    if(!ok) gta_record_clear(r);
    return ok;
}
/* Match the first TSV field in one bounded scan; URLs need no offset table
 * and may be edited or reordered without rebuilding anything. */
bool gta_record_find(
    Storage* storage,
    const char* asset,
    const char* key,
    size_t fields,
    GtaRecord* r) {
    gta_record_clear(r);
    if(!key || !*key) return false;
    File* file = open_catalog(storage, asset);
    if(!file) return false;
    uint64_t size = storage_file_size(file);
    uint8_t chunk[256];
    uint32_t pos = 0, start = 0;
    size_t key_len = strlen(key), matched = 0;
    bool prefix = true, equal = true, found = false;
    if(!size || size > 256u * 1024u) goto done;
    while(pos < size) {
        size_t n = size - pos > sizeof(chunk) ? sizeof(chunk) : (size_t)(size - pos);
        if(storage_file_read(file, chunk, n) != n) goto done;
        for(size_t i = 0; i < n; ++i, ++pos) {
            uint8_t ch = chunk[i];
            if(!ch || pos - start >= GTA_RECORD_MAX) goto done;
            if(ch == '\n') {
                if(!prefix && equal) {
                    size_t len = pos - start + 1u;
                    r->data = malloc(len);
                    found = r->data && storage_file_seek(file, start, true) &&
                            storage_file_read(file, r->data, len) == len &&
                            gta_record_decode(r, len, fields, true);
                    goto done;
                }
                start = pos + 1u;
                matched = 0;
                prefix = equal = true;
            } else if(prefix && ch == '\t') {
                prefix = false;
                equal = equal && matched == key_len;
            } else if(prefix && equal) {
                if(matched >= key_len || ch != (uint8_t)key[matched])
                    equal = false;
                else
                    ++matched;
            }
        }
    }
done:
    storage_file_free(file);
    if(!found) gta_record_clear(r);
    return found;
}
static GtaRecord cheat_records[2], screenshot_record, safe_record, secret_record;
static GtaCheat cheats[2];
static GtaScreenshotCode screenshot;
static GtaSafeCode safe;
static GtaSecretMission secret;
static bool read(const char* asset, size_t index, size_t fields, GtaRecord* r) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    bool ok = gta_record_read(s, asset, index, fields, r);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
const GtaCheat* gta_cheat_at(size_t index, bool detail, unsigned slot) {
    (void)detail;
    slot = slot ? 1 : 0;
    GtaRecord* r = &cheat_records[slot];
    if(!read("cheats.tsv", index, 7, r)) return NULL;
    char** f = r->field;
    cheats[slot] = (GtaCheat){f[0], f[1], f[2], f[3], f[4], f[5], f[6]};
    return &cheats[slot];
}
const GtaScreenshotCode* gta_screenshot_at(size_t index, bool detail) {
    (void)detail;
    if(!read("screenshots.tsv", index, 3, &screenshot_record)) return NULL;
    char** f = screenshot_record.field;
    screenshot = (GtaScreenshotCode){f[0], f[1], *f[2] ? f[2] : NULL};
    return &screenshot;
}
const GtaSafeCode* gta_safe_at(size_t index, bool detail) {
    (void)detail;
    if(!read("safes.tsv", index, 7, &safe_record)) return NULL;
    char** f = safe_record.field;
    if(strlen(f[5]) != 1 || *f[5] < '0' || *f[5] > '3' || strlen(f[6]) != 1 || *f[6] < '0' ||
       *f[6] > '4')
        return NULL;
    safe = (GtaSafeCode){
        f[0], f[1], f[2], f[3], f[4], (GtaSafeKind)(*f[5] - '0'), (GtaSafeStatus)(*f[6] - '0')};
    return &safe;
}
const GtaSecretMission* gta_secret_at(size_t index, bool detail) {
    (void)detail;
    if(!read("secrets.tsv", index, 9, &secret_record)) return NULL;
    char** f = secret_record.field;
    char* end;
    unsigned long id = strtoul(f[8], &end, 10);
    if(end == f[8] || *end || !id || id >= GtaSecretProgressBytes * 8u) return NULL;
    secret = (GtaSecretMission){f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], (uint8_t)id};
    return &secret;
}
void gta_catalog_release(void) {
    gta_record_index_release();
    gta_record_clear(&cheat_records[0]);
    gta_record_clear(&cheat_records[1]);
    gta_record_clear(&screenshot_record);
    gta_record_clear(&safe_record);
    gta_record_clear(&secret_record);
}
void gta_ram_log(const char* stage) {
#ifdef GTA_RAM_DIAGNOSTICS
    FURI_LOG_I(
        "GTA RAM",
        "%s: free=%zu minimum=%zu pool_largest=%zu stack_watermark=%lu",
        stage,
        memmgr_get_free_heap(),
        memmgr_get_minimum_free_heap(),
        memmgr_pool_get_max_block(),
        (unsigned long)furi_thread_get_stack_space(furi_thread_get_current_id()));
#else
    (void)stage;
#endif
}
/* Dispatch rows are short decimal triples. Keep only the next byte offset;
 * read a single bounded prefix and never allocate a Dispatch index. */
bool gta_dispatch_read(File* file, uint32_t* offset, GtaDispatchTarget* target) {
    uint64_t size = storage_file_size(file);
    if(*offset >= size || size > 256u * 1024u) return false;
    char row[24];
    size_t n = size - *offset < sizeof(row) ? (size_t)(size - *offset) : sizeof(row);
    if(!storage_file_seek(file, *offset, true) || storage_file_read(file, row, n) != n)
        return false;
    char* end = memchr(row, '\n', n);
    if(!end) return false;
    size_t used = (size_t)(end - row) + 1u;
    unsigned values[3] = {0};
    size_t field = 0, digits = 0;
    for(char* ch = row; ch < end; ++ch) {
        if(*ch == '\t') {
            if(!digits || ++field >= 3u) return false;
            digits = 0;
        } else {
            if(*ch < '0' || *ch > '9' || ++digits > 4u) return false;
            values[field] = values[field] * 10u + (unsigned)(*ch - '0');
        }
    }
    if(field != 2 || !digits || !values[0] || values[0] >= GtaProgressBytes * 8u ||
       values[1] >= gta_dispatch_source_count || !values[2] || values[2] > UINT8_MAX)
        return false;
    *target = (GtaDispatchTarget){(uint16_t)values[0], (uint8_t)values[1], (uint8_t)values[2]};
    *offset += (uint32_t)used;
    return true;
}
