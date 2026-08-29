#include "zeromesh_pmtiles.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "zeromesh_pmtiles"

#define PM_HEADER_LEN     127
#define PM_COMPRESSION_NONE 1
#define PM_TILETYPE_MVT     1

#define PM_MAX_ROOT_DIR (48 * 1024)

struct PmTiles {
    Storage* storage;
    File* file;

    uint64_t data_offset;
    uint32_t count;

    uint64_t* ids;
    uint32_t* offsets;
    uint32_t* lengths;
    uint32_t max_len;

    uint8_t min_zoom;
    uint8_t max_zoom;
};

static uint64_t rd_varint(const uint8_t* b, size_t len, size_t* p) {
    uint64_t r = 0;
    int s = 0;
    while(*p < len && s < 64) {
        uint8_t c = b[(*p)++];
        r |= (uint64_t)(c & 0x7f) << s;
        if(!(c & 0x80)) break;
        s += 7;
    }
    return r;
}

static uint64_t zxy_to_tileid(uint8_t z, uint32_t x, uint32_t y) {

    uint64_t acc = (((uint64_t)1 << (z * 2)) - 1) / 3;
    uint64_t d = 0;
    uint32_t tx = x, ty = y;
    for(uint32_t s = (uint32_t)1 << (z ? z - 1 : 0); z && s > 0; s >>= 1) {
        uint32_t rx = (tx & s) ? 1 : 0;
        uint32_t ry = (ty & s) ? 1 : 0;
        d += (uint64_t)s * s * ((3 * rx) ^ ry);

        if(ry == 0) {
            if(rx == 1) {
                tx = s - 1 - tx;
                ty = s - 1 - ty;
            }
            uint32_t t = tx;
            tx = ty;
            ty = t;
        }
    }
    return acc + d;
}

static bool read_at(File* f, uint64_t off, void* dst, size_t len) {
    if(!storage_file_seek(f, (uint32_t)off, true)) return false;
    uint8_t* p = dst;
    size_t done = 0;
    while(done < len) {
        size_t want = len - done;
        if(want > 0x8000) want = 0x8000;
        uint16_t got = storage_file_read(f, p + done, (uint16_t)want);
        if(got == 0) return false;
        done += got;
    }
    return true;
}

PmTiles* pmtiles_open(const char* path) {
    PmTiles* p = malloc(sizeof(PmTiles));
    if(!p) return NULL;
    memset(p, 0, sizeof(PmTiles));

    p->storage = furi_record_open(RECORD_STORAGE);
    p->file = storage_file_alloc(p->storage);

    uint8_t* dir = NULL;

    if(!storage_file_open(p->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_W(TAG, "cannot open %s", path);
        goto fail;
    }

    uint8_t h[PM_HEADER_LEN];
    if(!read_at(p->file, 0, h, sizeof(h))) {
        FURI_LOG_E(TAG, "short header");
        goto fail;
    }
    if(memcmp(h, "PMTiles", 7) != 0 || h[7] != 3) {
        FURI_LOG_E(TAG, "not a PMTiles v3 archive");
        goto fail;
    }
    if(h[97] != PM_COMPRESSION_NONE || h[98] != PM_COMPRESSION_NONE) {
        FURI_LOG_E(
            TAG, "compressed archive (internal=%u tile=%u); rebuild with none", h[97], h[98]);
        goto fail;
    }
    if(h[99] != PM_TILETYPE_MVT) {
        FURI_LOG_E(TAG, "tile type %u is not MVT", h[99]);
        goto fail;
    }

    uint64_t root_off, root_len, leaf_len;
    memcpy(&root_off, h + 8, 8);
    memcpy(&root_len, h + 16, 8);
    memcpy(&leaf_len, h + 48, 8);
    memcpy(&p->data_offset, h + 56, 8);
    p->min_zoom = h[100];
    p->max_zoom = h[101];

    if(leaf_len != 0) {
        FURI_LOG_E(TAG, "archive uses leaf directories, unsupported");
        goto fail;
    }
    if(root_len == 0 || root_len > PM_MAX_ROOT_DIR) {
        FURI_LOG_E(TAG, "root dir %lu bytes out of range", (unsigned long)root_len);
        goto fail;
    }

    dir = malloc((size_t)root_len);
    if(!dir || !read_at(p->file, root_off, dir, (size_t)root_len)) {
        FURI_LOG_E(TAG, "cannot read root dir");
        goto fail;
    }

    size_t q = 0;
    uint64_t n = rd_varint(dir, (size_t)root_len, &q);
    if(n == 0 || n > 100000) {
        FURI_LOG_E(TAG, "entry count %lu implausible", (unsigned long)n);
        goto fail;
    }
    p->count = (uint32_t)n;

    p->ids = malloc(sizeof(uint64_t) * p->count);
    p->offsets = malloc(sizeof(uint32_t) * p->count);
    p->lengths = malloc(sizeof(uint32_t) * p->count);
    if(!p->ids || !p->offsets || !p->lengths) {
        FURI_LOG_E(TAG, "out of memory for %lu entries", (unsigned long)n);
        goto fail;
    }

    uint64_t last = 0;
    for(uint32_t i = 0; i < p->count; i++) {
        last += rd_varint(dir, (size_t)root_len, &q);
        p->ids[i] = last;
    }
    for(uint32_t i = 0; i < p->count; i++) {
        rd_varint(dir, (size_t)root_len, &q);
    }
    for(uint32_t i = 0; i < p->count; i++) {
        p->lengths[i] = (uint32_t)rd_varint(dir, (size_t)root_len, &q);
        if(p->lengths[i] > p->max_len) p->max_len = p->lengths[i];
    }
    for(uint32_t i = 0; i < p->count; i++) {
        uint64_t v = rd_varint(dir, (size_t)root_len, &q);

        p->offsets[i] = (v == 0) ? (i ? p->offsets[i - 1] + p->lengths[i - 1] : 0)
                                 : (uint32_t)(v - 1);
    }

    free(dir);
    FURI_LOG_I(
        TAG,
        "opened %s: %lu tiles z%u-%u, max tile %lu b",
        path,
        (unsigned long)p->count,
        p->min_zoom,
        p->max_zoom,
        (unsigned long)p->max_len);
    return p;

fail:
    free(dir);
    pmtiles_close(p);
    return NULL;
}

void pmtiles_close(PmTiles* p) {
    if(!p) return;
    free(p->ids);
    free(p->offsets);
    free(p->lengths);
    if(p->file) {
        storage_file_close(p->file);
        storage_file_free(p->file);
    }
    if(p->storage) furi_record_close(RECORD_STORAGE);
    free(p);
}

bool pmtiles_get_tile(
    PmTiles* p,
    uint8_t z,
    uint32_t x,
    uint32_t y,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!p || !out || !out_len) return false;
    *out_len = 0;
    if(z < p->min_zoom || z > p->max_zoom) return false;

    uint64_t want = zxy_to_tileid(z, x, y);

    uint32_t lo = 0, hi = p->count;
    while(lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(p->ids[mid] < want) {
            lo = mid + 1;
        } else if(p->ids[mid] > want) {
            hi = mid;
        } else {
            uint32_t len = p->lengths[mid];
            if(len > out_cap) {
                FURI_LOG_W(TAG, "tile %lu b exceeds buffer %u", (unsigned long)len, (unsigned)out_cap);
                return false;
            }
            if(!read_at(p->file, p->data_offset + p->offsets[mid], out, len)) return false;
            *out_len = len;
            return true;
        }
    }
    return false;
}

uint8_t pmtiles_min_zoom(const PmTiles* p) {
    return p ? p->min_zoom : 0;
}
uint8_t pmtiles_max_zoom(const PmTiles* p) {
    return p ? p->max_zoom : 0;
}
uint32_t pmtiles_tile_count(const PmTiles* p) {
    return p ? p->count : 0;
}
uint32_t pmtiles_max_tile_len(const PmTiles* p) {
    return p ? p->max_len : 0;
}
