#include "proto_encode.h"

size_t proto_line_cap(size_t cap) {
    /* PROTO_LINE_MAX - 1 bytes of text plus the writer's NUL. */
    return cap > PROTO_LINE_MAX ? PROTO_LINE_MAX : cap;
}

bool proto_id_valid(const char* id) {
    size_t n = 0;
    for(; id[n]; n++) {
        char c = id[n];
        bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':' || c == '-';
        if(!allowed || n >= PROTO_ID_MAX) return false;
    }
    return n > 0;
}

bool proto_write_header(JsonWriter* w, const char* type) {
    json_key(w, "t");
    json_string(w, type);
    json_key(w, "v");
    return json_int(w, PROTO_VERSION);
}

/* Writes a decimal object key such as "12". */
static bool proto_number_key(JsonWriter* w, unsigned n) {
    char key[4];
    size_t len = 0;
    if(n >= 100) key[len++] = (char)('0' + n / 100);
    if(n >= 10) key[len++] = (char)('0' + n / 10 % 10);
    key[len++] = (char)('0' + n % 10);
    key[len] = '\0';
    return json_key(w, key);
}

static bool proto_write_mfc_data(JsonWriter* w, const TagDump* dump) {
    json_key(w, "blocks");
    json_object_open(w);
    for(uint8_t b = 0; b < TAG_DUMP_MFC_BLOCKS; b++) {
        bool trailer = b % TAG_DUMP_MFC_BLOCKS_PER_SECTOR == TAG_DUMP_MFC_BLOCKS_PER_SECTOR - 1;
        if(trailer || !tag_dump_block_present(dump, b)) continue;
        proto_number_key(w, b);
        json_hex(w, dump->blocks[b], TAG_DUMP_BLOCK_LEN);
    }
    json_object_close(w);

    bool any_missing = false;
    for(uint8_t s = 0; s < TAG_DUMP_MFC_SECTORS; s++) {
        if(!tag_dump_sector_missing(dump, s)) continue;
        if(!any_missing) {
            json_key(w, "missing");
            json_array_open(w);
            any_missing = true;
        }
        json_int(w, s);
    }
    if(any_missing) json_array_close(w);
    return json_ok(w);
}

static bool proto_write_ntag_data(JsonWriter* w, const TagDump* dump) {
    json_key(w, "pages");
    json_object_open(w);
    for(uint16_t p = 0; p < dump->page_count; p++) {
        proto_number_key(w, p);
        json_hex(w, dump->pages[p], TAG_DUMP_PAGE_LEN);
    }
    return json_object_close(w);
}

bool proto_write_tag(JsonWriter* w, const TagDump* dump) {
    if(!tag_dump_valid(dump)) return false;
    json_object_open(w);
    json_key(w, "type");
    json_string(w, dump->type == TagTypeMfc ? "mfc" : "ntag");
    json_key(w, "uid");
    json_hex(w, dump->uid, dump->uid_len);
    json_key(w, "sak");
    json_int(w, dump->sak);
    json_key(w, "atqa");
    json_hex(w, dump->atqa, sizeof(dump->atqa));
    if(dump->type == TagTypeMfc)
        proto_write_mfc_data(w, dump);
    else
        proto_write_ntag_data(w, dump);
    return json_object_close(w);
}

size_t proto_encode_hello(const char* app, const char* fw, char* buf, size_t cap) {
    JsonWriter w;
    json_init(&w, buf, proto_line_cap(cap));
    json_object_open(&w);
    proto_write_header(&w, "hello");
    json_key(&w, "app");
    json_string(&w, app);
    json_key(&w, "fw");
    json_string(&w, fw);
    json_object_close(&w);
    return json_len(&w);
}

size_t proto_encode_scan(
    const TagDump* dump,
    const char* scan_id,
    bool autosave,
    char* buf,
    size_t cap) {
    if(!proto_id_valid(scan_id)) return 0;
    JsonWriter w;
    json_init(&w, buf, proto_line_cap(cap));
    json_object_open(&w);
    proto_write_header(&w, "scan");
    json_key(&w, "scan_id");
    json_string(&w, scan_id);
    json_key(&w, "autosave");
    json_bool(&w, autosave);
    json_key(&w, "tag");
    if(!proto_write_tag(&w, dump)) return 0;
    json_object_close(&w);
    return json_len(&w);
}
