#include "nus_charpack.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "NusCharpack"

#define PACKS_PARENT  "/ext/apps_data/claude_buddy/packs"
#define PACK_NAME_MAX 32
#define PACK_PATH_MAX 96

static Storage* s_storage = NULL;
static File* s_file = NULL;
static char s_pack_dir[PACK_PATH_MAX];
static int32_t s_file_bytes;

/* ── base64 decoder (RFC 4648, no line breaks, `=` padding) ─────── */

static const uint8_t B64_TABLE[256] = {
    /* 0x00 .. 0x1F */
    ['+'] = 62, ['/'] = 63,
    ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55, ['4'] = 56,
    ['5'] = 57, ['6'] = 58, ['7'] = 59, ['8'] = 60, ['9'] = 61,
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,  ['E'] = 4,
    ['F'] = 5,  ['G'] = 6,  ['H'] = 7,  ['I'] = 8,  ['J'] = 9,
    ['K'] = 10, ['L'] = 11, ['M'] = 12, ['N'] = 13, ['O'] = 14,
    ['P'] = 15, ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19,
    ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23, ['Y'] = 24, ['Z'] = 25,
    ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29, ['e'] = 30,
    ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
    ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40,
    ['p'] = 41, ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45,
    ['u'] = 46, ['v'] = 47, ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51,
};

/* Decode up to b64_len chars from src into dst.  Returns bytes written,
 * or -1 on malformed input. Ignores trailing whitespace. */
static int b64_decode(const char* src, int b64_len, uint8_t* dst, int dst_cap) {
    int written = 0;
    uint32_t buf = 0;
    int bits = 0;
    int i = 0;
    while(i < b64_len) {
        char c = src[i++];
        if(c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if(c == '=') break; /* padding — stop consuming real bytes */
        uint8_t v = B64_TABLE[(uint8_t)c];
        if(v == 0 && c != 'A') return -1;
        buf = (buf << 6) | v;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            if(written >= dst_cap) return -1;
            dst[written++] = (uint8_t)((buf >> bits) & 0xff);
        }
    }
    return written;
}

/* ── path validation ────────────────────────────────────────── */

/* Reject leading `/`, embedded `..` components, null bytes, or any
 * character outside a conservative safe set. */
static bool path_is_safe(const char* path) {
    if(!path || !path[0]) return false;
    if(path[0] == '/') return false;
    int len = (int)strlen(path);
    if(len >= PACK_PATH_MAX) return false;
    /* Walk components between `/`. */
    int start = 0;
    for(int i = 0; i <= len; i++) {
        if(path[i] == '/' || path[i] == '\0') {
            int clen = i - start;
            if(clen == 0) return false;
            if(clen == 2 && path[start] == '.' && path[start + 1] == '.') return false;
            if(clen == 1 && path[start] == '.') return false;
            start = i + 1;
        } else {
            unsigned char c = (unsigned char)path[i];
            /* Allow alnum, dot, dash, underscore — restrictive on purpose. */
            bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                      (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '_';
            if(!ok) return false;
        }
    }
    return true;
}

static bool name_is_safe(const char* name) {
    if(!name || !name[0]) return false;
    int len = (int)strlen(name);
    if(len >= PACK_NAME_MAX) return false;
    for(int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') || c == '.' || c == '-' || c == '_';
        if(!ok) return false;
    }
    return true;
}

/* ── public API ─────────────────────────────────────────────── */

void nus_charpack_init(void) {
    if(!s_storage) s_storage = furi_record_open(RECORD_STORAGE);
    if(!s_file) s_file = storage_file_alloc(s_storage);
    s_pack_dir[0] = '\0';
    s_file_bytes = 0;
}

void nus_charpack_free(void) {
    if(s_file) {
        if(storage_file_is_open(s_file)) storage_file_close(s_file);
        storage_file_free(s_file);
        s_file = NULL;
    }
    if(s_storage) {
        furi_record_close(RECORD_STORAGE);
        s_storage = NULL;
    }
}

void nus_charpack_reset(void) {
    if(s_file && storage_file_is_open(s_file)) {
        storage_file_close(s_file);
    }
    s_pack_dir[0] = '\0';
    s_file_bytes = 0;
}

bool nus_charpack_begin(const char* name) {
    if(!s_storage) return false;
    if(!name_is_safe(name)) {
        FURI_LOG_W(TAG, "rejecting unsafe pack name");
        return false;
    }
    nus_charpack_reset();

    /* Ensure parent chain exists before the pack-specific dir. */
    storage_simply_mkdir(s_storage, "/ext/apps_data");
    storage_simply_mkdir(s_storage, "/ext/apps_data/claude_buddy");
    storage_simply_mkdir(s_storage, PACKS_PARENT);

    snprintf(s_pack_dir, sizeof(s_pack_dir), "%s/%s", PACKS_PARENT, name);
    storage_simply_mkdir(s_storage, s_pack_dir);
    FURI_LOG_I(TAG, "pack dir: %s", s_pack_dir);
    return true;
}

bool nus_charpack_file_open(const char* path) {
    if(!s_storage || !s_file) return false;
    if(!s_pack_dir[0]) return false;
    if(!path_is_safe(path)) {
        FURI_LOG_W(TAG, "rejecting unsafe path: %s", path);
        return false;
    }
    if(storage_file_is_open(s_file)) storage_file_close(s_file);

    char full[PACK_PATH_MAX * 2];
    snprintf(full, sizeof(full), "%s/%s", s_pack_dir, path);
    bool ok = storage_file_open(s_file, full, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) FURI_LOG_E(TAG, "open fail: %s", full);
    s_file_bytes = 0;
    return ok;
}

int32_t nus_charpack_chunk_write(const char* b64, int b64_len) {
    if(!s_file || !storage_file_is_open(s_file)) return -1;
    /* Base64 expands 4→3 bytes; grow-down estimate suits us. */
    uint8_t buf[384];
    int cap = sizeof(buf);
    int n = b64_decode(b64, b64_len, buf, cap);
    if(n < 0) {
        FURI_LOG_W(TAG, "bad base64 chunk (len=%d)", b64_len);
        return -1;
    }
    uint16_t wrote = storage_file_write(s_file, buf, (uint16_t)n);
    s_file_bytes += wrote;
    return s_file_bytes;
}

int32_t nus_charpack_file_close(void) {
    if(!s_file || !storage_file_is_open(s_file)) return -1;
    storage_file_close(s_file);
    int32_t size = s_file_bytes;
    s_file_bytes = 0;
    return size;
}

void nus_charpack_end(void) {
    /* No-op at the moment — pack is already on disk after each file_end. */
    if(s_pack_dir[0]) FURI_LOG_I(TAG, "pack finalised: %s", s_pack_dir);
}
