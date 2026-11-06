#include "proto_decode.h"

#include <string.h>

#include "json_reader.h"

#define PROTO_KEY_MAX  16 /* longer than any known key; longer keys are skipped */
#define PROTO_TYPE_MAX 32

static bool key_is(const char* key, const char* name) {
    return strcmp(key, name) == 0;
}

/* A non-empty string of at most `cap - 1` characters. */
static bool read_text(JsonReader* r, char* out, size_t cap) {
    return json_read_string(r, out, cap) && out[0] != '\0';
}

static bool read_id(JsonReader* r, char out[PROTO_ID_MAX + 1]) {
    return json_read_string(r, out, PROTO_ID_MAX + 1) && proto_id_valid(out);
}

static bool read_grams(JsonReader* r, bool* has, uint16_t* out) {
    long v;
    if(!json_read_int(r, &v) || v < 0 || v > PROTO_GRAMS_MAX) return false;
    *has = true;
    *out = (uint16_t)v;
    return true;
}

static bool read_temp(JsonReader* r, bool* has, uint16_t* out) {
    long v;
    if(!json_read_int(r, &v) || v < 0 || v > PROTO_TEMP_MAX) return false;
    *has = true;
    *out = (uint16_t)v;
    return true;
}

/* Exactly eight upper-case hex digits, RRGGBBAA. */
static bool read_rgba(JsonReader* r, bool* has, uint32_t* out) {
    char s[9];
    if(!json_read_string(r, s, sizeof(s)) || strlen(s) != 8) return false;
    uint32_t v = 0;
    for(size_t i = 0; i < 8; i++) {
        char c = s[i];
        unsigned d;
        if(c >= '0' && c <= '9')
            d = (unsigned)(c - '0');
        else if(c >= 'A' && c <= 'F')
            d = (unsigned)(c - 'A' + 10);
        else
            return false;
        v = v << 4 | d;
    }
    *has = true;
    *out = v;
    return true;
}

static bool read_spool_id(JsonReader* r, int32_t* out) {
    long v;
    if(!json_read_int(r, &v) || v < 1 || v > INT32_MAX) return false;
    *out = (int32_t)v;
    return true;
}

static bool read_lines(JsonReader* r, ProtoLines* lines) {
    memset(lines, 0, sizeof(*lines));
    if(!json_array_begin(r)) return false;
    while(json_array_next(r)) {
        if(lines->count == PROTO_LINES_MAX) return false;
        if(!json_read_string(r, lines->line[lines->count], PROTO_LINE_CHARS + 1)) return false;
        lines->count++;
    }
    return json_reader_ok(r);
}

static bool read_defaults(JsonReader* r, ProtoDefaults* d) {
    char key[PROTO_KEY_MAX];
    memset(d, 0, sizeof(*d));
    if(!json_object_begin(r)) return false;
    while(json_object_next(r, key, sizeof(key))) {
        bool ok;
        if(key_is(key, "gross"))
            ok = read_grams(r, &d->has_gross, &d->gross);
        else if(key_is(key, "label"))
            ok = read_grams(r, &d->has_label, &d->label);
        else if(key_is(key, "core"))
            ok = read_grams(r, &d->has_core, &d->core);
        else
            ok = json_skip(r);
        if(!ok) return false;
    }
    return json_reader_ok(r);
}

enum {
    SpoolSeenId = 1 << 0,
    SpoolSeenMaterial = 1 << 1,
    SpoolSeenRemaining = 1 << 2,
    SpoolSeenLabel = 1 << 3,
    SpoolSeenCore = 1 << 4,
    SpoolSeenRequired = (1 << 5) - 1,
};

static bool read_spool_field(JsonReader* r, const char* key, ProtoSpool* s, unsigned* seen) {
    bool has; /* the required grams are always present once seen */
    if(key_is(key, "id")) {
        *seen |= SpoolSeenId;
        return read_spool_id(r, &s->id);
    }
    if(key_is(key, "material")) {
        *seen |= SpoolSeenMaterial;
        return read_text(r, s->material, sizeof(s->material));
    }
    if(key_is(key, "subtype")) return read_text(r, s->subtype, sizeof(s->subtype));
    if(key_is(key, "color_name")) return read_text(r, s->color_name, sizeof(s->color_name));
    if(key_is(key, "rgba")) return read_rgba(r, &s->has_rgba, &s->rgba);
    if(key_is(key, "remaining")) {
        *seen |= SpoolSeenRemaining;
        return read_grams(r, &has, &s->remaining);
    }
    if(key_is(key, "label")) {
        *seen |= SpoolSeenLabel;
        return read_grams(r, &has, &s->label);
    }
    if(key_is(key, "core")) {
        *seen |= SpoolSeenCore;
        return read_grams(r, &has, &s->core);
    }
    if(key_is(key, "archived")) return json_read_bool(r, &s->archived);
    return json_skip(r);
}

static bool read_spool(JsonReader* r, ProtoSpool* s) {
    char key[PROTO_KEY_MAX];
    unsigned seen = 0;
    memset(s, 0, sizeof(*s));
    if(!json_object_begin(r)) return false;
    while(json_object_next(r, key, sizeof(key)))
        if(!read_spool_field(r, key, s, &seen)) return false;
    return json_reader_ok(r) && seen == SpoolSeenRequired;
}

static bool read_tag_info(JsonReader* r, ProtoTagInfo* t) {
    char key[PROTO_KEY_MAX];
    memset(t, 0, sizeof(*t));
    if(!json_object_begin(r)) return false;
    while(json_object_next(r, key, sizeof(key))) {
        bool ok;
        if(key_is(key, "material"))
            ok = read_text(r, t->material, sizeof(t->material));
        else if(key_is(key, "subtype"))
            ok = read_text(r, t->subtype, sizeof(t->subtype));
        else if(key_is(key, "rgba"))
            ok = read_rgba(r, &t->has_rgba, &t->rgba);
        else if(key_is(key, "label"))
            ok = read_grams(r, &t->has_label, &t->label);
        else if(key_is(key, "color_name"))
            ok = read_text(r, t->color_name, sizeof(t->color_name));
        else if(key_is(key, "code"))
            ok = read_text(r, t->code, sizeof(t->code));
        else if(key_is(key, "filament_id"))
            ok = read_text(r, t->filament_id, sizeof(t->filament_id));
        else if(key_is(key, "diameter"))
            ok = read_text(r, t->diameter, sizeof(t->diameter));
        else if(key_is(key, "nozzle_min"))
            ok = read_temp(r, &t->has_nozzle_min, &t->nozzle_min);
        else if(key_is(key, "nozzle_max"))
            ok = read_temp(r, &t->has_nozzle_max, &t->nozzle_max);
        else
            ok = json_skip(r);
        if(!ok) return false;
    }
    return json_reader_ok(r);
}

/* Required-field bookkeeping while walking the fields of one message. */
enum {
    SeenId = 1 << 0, /* scan_id for lookup, id for result */
    SeenStatus = 1 << 1,
    SeenOk = 1 << 2,
    SeenLines = 1 << 3,
    SeenApp = 1 << 4,
    SeenFw = 1 << 5,
    SeenOs = 1 << 6,
    SeenSpoolId = 1 << 7,
    SeenError = 1 << 8,
    SeenSpool = 1 << 9,
    SeenCreated = 1 << 10,
    SeenTagInfo = 1 << 11,
};

static bool decode_hello_field(JsonReader* r, const char* key, ProtoHello* h, unsigned* seen) {
    if(key_is(key, "app")) {
        *seen |= SeenApp;
        return read_text(r, h->app, sizeof(h->app));
    }
    if(key_is(key, "fw")) {
        *seen |= SeenFw;
        return read_text(r, h->fw, sizeof(h->fw));
    }
    if(key_is(key, "os")) {
        *seen |= SeenOs;
        return read_text(r, h->os, sizeof(h->os));
    }
    return json_skip(r);
}

static bool hello_complete(unsigned seen) {
    bool fw = seen & SeenFw;
    bool os = seen & SeenOs;
    return (seen & SeenApp) && fw != os;
}

static bool read_lookup_status(JsonReader* r, ProtoLookupStatus* out) {
    char s[8];
    if(!json_read_string(r, s, sizeof(s))) return false;
    if(key_is(s, "found"))
        *out = ProtoLookupFound;
    else if(key_is(s, "unknown"))
        *out = ProtoLookupUnknown;
    else if(key_is(s, "error"))
        *out = ProtoLookupError;
    else
        return false;
    return true;
}

static bool decode_lookup_field(JsonReader* r, const char* key, ProtoLookup* l, unsigned* seen) {
    if(key_is(key, "scan_id")) {
        *seen |= SeenId;
        return read_id(r, l->scan_id);
    }
    if(key_is(key, "status")) {
        *seen |= SeenStatus;
        return read_lookup_status(r, &l->status);
    }
    if(key_is(key, "spool_id")) {
        *seen |= SeenSpoolId;
        return read_spool_id(r, &l->spool_id);
    }
    if(key_is(key, "prefill")) return json_read_bool(r, &l->prefill);
    if(key_is(key, "spool")) {
        *seen |= SeenSpool;
        return l->has_spool = read_spool(r, &l->spool);
    }
    if(key_is(key, "created")) {
        *seen |= SeenCreated;
        return json_read_bool(r, &l->created);
    }
    if(key_is(key, "tag_info")) {
        *seen |= SeenTagInfo;
        return l->has_tag_info = read_tag_info(r, &l->tag_info);
    }
    if(key_is(key, "lines")) {
        *seen |= SeenLines;
        return read_lines(r, &l->lines);
    }
    if(key_is(key, "defaults")) return read_defaults(r, &l->defaults);
    if(key_is(key, "error")) {
        *seen |= SeenError;
        return read_text(r, l->error, sizeof(l->error));
    }
    return json_skip(r);
}

static bool lookup_complete(const ProtoLookup* l, unsigned seen) {
    unsigned required = SeenId | SeenStatus | SeenLines;
    if((seen & required) != required) return false;
    if(l->status == ProtoLookupFound && !(seen & SeenSpoolId)) return false;
    if(l->status == ProtoLookupError && !(seen & SeenError)) return false;
    if(l->status != ProtoLookupFound && (seen & (SeenSpool | SeenCreated))) return false;
    if(l->status != ProtoLookupUnknown && (seen & SeenTagInfo)) return false;
    return !l->has_spool || l->spool.id == l->spool_id;
}

static bool decode_result_field(JsonReader* r, const char* key, ProtoResult* res, unsigned* seen) {
    if(key_is(key, "id")) {
        *seen |= SeenId;
        return read_id(r, res->id);
    }
    if(key_is(key, "ok")) {
        *seen |= SeenOk;
        return json_read_bool(r, &res->ok);
    }
    if(key_is(key, "lines")) {
        *seen |= SeenLines;
        return read_lines(r, &res->lines);
    }
    if(key_is(key, "spool")) {
        *seen |= SeenSpool;
        return res->has_spool = read_spool(r, &res->spool);
    }
    if(key_is(key, "error")) {
        *seen |= SeenError;
        return read_text(r, res->error, sizeof(res->error));
    }
    return json_skip(r);
}

static bool result_complete(const ProtoResult* res, unsigned seen) {
    unsigned required = SeenId | SeenOk | SeenLines;
    if((seen & required) != required) return false;
    if(res->ok) return true;
    return (seen & SeenError) && !(seen & SeenSpool);
}

#define PROTO_OTHER_VERSION_MAX 99

/* First pass: validates the whole line as one JSON object and picks up
 * `t` and `v`, which may come anywhere in it. */
static ProtoDecodeStatus
    read_header(const char* line, size_t len, ProtoMsgType* type, uint8_t* other_version) {
    JsonReader r;
    char key[PROTO_KEY_MAX];
    char t[PROTO_TYPE_MAX];
    bool has_t = false;
    bool has_v = false;
    long v = 0;
    json_reader_init(&r, line, len);
    if(!json_object_begin(&r)) return ProtoDecodeMalformed;
    while(json_object_next(&r, key, sizeof(key))) {
        bool ok;
        if(key_is(key, "t"))
            ok = has_t = json_read_string(&r, t, sizeof(t));
        else if(key_is(key, "v"))
            ok = has_v = json_read_int(&r, &v);
        else
            ok = json_skip(&r);
        if(!ok) return ProtoDecodeMalformed;
    }
    if(!json_reader_finish(&r) || !has_t || !has_v) return ProtoDecodeMalformed;
    if(v != PROTO_VERSION) {
        if(key_is(t, "hello") && v >= 1 && v <= PROTO_OTHER_VERSION_MAX) {
            *other_version = (uint8_t)v;
            return ProtoDecodeOtherVersion;
        }
        return ProtoDecodeIgnored;
    }
    if(key_is(t, "hello"))
        *type = ProtoMsgHello;
    else if(key_is(t, "lookup"))
        *type = ProtoMsgLookup;
    else if(key_is(t, "result"))
        *type = ProtoMsgResult;
    else
        return ProtoDecodeIgnored;
    return ProtoDecodeOk;
}

/* Second pass: the fields of a message whose type is known. */
static bool decode_body(const char* line, size_t len, ProtoMessage* msg) {
    JsonReader r;
    char key[PROTO_KEY_MAX];
    unsigned seen = 0;
    json_reader_init(&r, line, len);
    if(!json_object_begin(&r)) return false;
    while(json_object_next(&r, key, sizeof(key))) {
        bool ok;
        if(key_is(key, "t") || key_is(key, "v"))
            ok = json_skip(&r);
        else if(msg->type == ProtoMsgHello)
            ok = decode_hello_field(&r, key, &msg->hello, &seen);
        else if(msg->type == ProtoMsgLookup)
            ok = decode_lookup_field(&r, key, &msg->lookup, &seen);
        else
            ok = decode_result_field(&r, key, &msg->result, &seen);
        if(!ok) return false;
    }
    if(!json_reader_finish(&r)) return false;
    switch(msg->type) {
    case ProtoMsgHello:
        return hello_complete(seen);
    case ProtoMsgLookup:
        return lookup_complete(&msg->lookup, seen);
    case ProtoMsgResult:
        return result_complete(&msg->result, seen);
    default:
        return false;
    }
}

ProtoDecodeStatus proto_decode_line(const char* line, ProtoMessage* msg) {
    memset(msg, 0, sizeof(*msg));
    size_t len = strlen(line);
    ProtoMsgType type = ProtoMsgNone;
    uint8_t other_version = 0;
    ProtoDecodeStatus status = read_header(line, len, &type, &other_version);
    if(status == ProtoDecodeOtherVersion) {
        msg->type = ProtoMsgOtherVersion;
        msg->other_version = other_version;
        return status;
    }
    if(status != ProtoDecodeOk) return status;
    msg->type = type;
    if(!decode_body(line, len, msg)) {
        memset(msg, 0, sizeof(*msg));
        return ProtoDecodeMalformed;
    }
    return ProtoDecodeOk;
}
