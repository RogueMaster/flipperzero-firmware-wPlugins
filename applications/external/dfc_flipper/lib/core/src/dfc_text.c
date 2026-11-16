#include "dfc_text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// File type codes, from the table in section 1.5.

#if DFC_ENABLE_TEXT_CODEC

bool dfc_credential_content_is_binary(const uint8_t* content, size_t len) {
    return content && len > 0 && content[0] == DFC_DER_CREDENTIAL_TAG;
}

#define TYPE_STANDARD 0x00u
#define TYPE_BACKUP   0x01u
#define TYPE_VALUE    0x02u
#define TYPE_LINEAR   0x03u
#define TYPE_CYCLIC   0x04u
#define TYPE_TMAC     0x05u

#define U24_MAX 16777215u
#define U32_MAX 4294967295u

// Key spelling buffers. A parent is "PICC" or "Application NN", a prefix adds
// " File FF", and a key adds the longest suffix of the section 2.1.6 table.
#define PARENT_MAX 16
#define PREFIX_MAX 40
#define KEY_MAX    96

static const char* const GENERATION_NAMES[] = {"EV1", "EV2", "EV3"};
static const char* const PROVENANCE_NAMES[] = {"Real", "Random", "Unknown"};
static const char* const AUTH_NAMES[] = {"D40", "ISO", "AES"};
static const char* const AUTH_COMMAND_NAMES[] =
    {"D40", "ISO", "AES", "EV2First", "EV2NonFirst", "ISO7816"};
static const uint8_t AUTH_COMMAND_BITS[] = {
    DFC_AUTH_COMMAND_D40,
    DFC_AUTH_COMMAND_ISO_NATIVE,
    DFC_AUTH_COMMAND_AES,
    DFC_AUTH_COMMAND_EV2_FIRST,
    DFC_AUTH_COMMAND_EV2_NON_FIRST,
    DFC_AUTH_COMMAND_ISO7816};
static const char* const FILE_TYPE_NAMES[] =
    {"Standard Data", "Backup Data", "Value", "Linear Record", "Cyclic Record", "Transaction MAC"};

const char* dfc_text_status_name(DfcTextStatus status) {
    return dfc_der_status_name(status);
}

static bool type_is_data(uint8_t type) {
    return type == TYPE_STANDARD || type == TYPE_BACKUP;
}

static bool type_is_record(uint8_t type) {
    return type == TYPE_LINEAR || type == TYPE_CYCLIC;
}

static uint8_t auth_command_for(size_t index) {
    if(index == 2) return DFC_CMD_AUTHENTICATE_AES;
    if(index == 1) return DFC_CMD_AUTHENTICATE_ISO;
    return DFC_CMD_AUTHENTICATE_LEGACY;
}

// Stored key length is fixed by the key type (section 1.4); 8-octet DES is held
// as 16, which is what the model already carries.
// ------------------------------------------------------------------ parser ---

typedef struct {
    const char* key;
    size_t key_len;
    const char* value;
    size_t value_len;
    size_t number;
} Line;

typedef struct {
    const char* text;
    size_t len;
    DfcCredential* c;
    unsigned format_version;

    // Significant lines seen, and how many of them a rule claimed. Every claim
    // targets a distinct key, so equality at the end means every line was
    // recognised (rule 4).
    size_t total;
    size_t consumed;
    size_t cursor_pos;
    size_t cursor_line;
    size_t lookahead_end;
    Line lookahead;
    const char* last_key;
    size_t last_key_len;
    bool has_lookahead;

    bool failed;
    DfcTextStatus status;
    DfcTextError err;
} P;

static bool fail(P* p, size_t line, const char* fmt, ...);
static bool required(P* p, const char* key, Line* line);

static bool req_auth_commands(P* p, const char* key, uint8_t* commands) {
    Line line;
    if(!required(p, key, &line)) return false;
    if(line.value_len == 4 && memcmp(line.value, "None", 4) == 0) {
        *commands = 0;
        return true;
    }
    size_t at = 0;
    uint8_t result = 0;
    for(size_t i = 0; i < sizeof(AUTH_COMMAND_BITS); i++) {
        size_t length = strlen(AUTH_COMMAND_NAMES[i]);
        if(at + length <= line.value_len &&
           memcmp(line.value + at, AUTH_COMMAND_NAMES[i], length) == 0 &&
           (at + length == line.value_len || (at + length + 2 <= line.value_len &&
                                              memcmp(line.value + at + length, ", ", 2) == 0))) {
            result |= AUTH_COMMAND_BITS[i];
            at += length;
            if(at == line.value_len) break;
            at += 2;
        }
    }
    if(at != line.value_len || result == 0) {
        return fail(p, line.number, "%s: invalid command set or order", key);
    }
    *commands = result;
    return true;
}

static bool fail(P* p, size_t line, const char* fmt, ...) {
    if(!p->failed) {
        p->failed = true;
        p->status = DfcTextMalformed;
        p->err.line = line;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(p->err.message, sizeof(p->err.message), fmt, ap);
        va_end(ap);
    }
    return false;
}

static bool fail_class(P* p, DfcTextStatus status, size_t line, const char* fmt, ...) {
    if(!p->failed) {
        p->failed = true;
        p->status = status;
        p->err.line = line;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(p->err.message, sizeof(p->err.message), fmt, ap);
        va_end(ap);
    }
    return false;
}

// Next raw line, LF terminated. A final line without its LF is still a line.
static bool raw_line(const P* p, size_t* pos, const char** start, size_t* n) {
    if(*pos >= p->len) return false;
    const char* s = p->text + *pos;
    size_t remaining = p->len - *pos;
    const char* nl = memchr(s, '\n', remaining);
    size_t l = nl ? (size_t)(nl - s) : remaining;
    *start = s;
    *n = l;
    *pos += nl ? l + 1 : l;
    return true;
}

static bool line_is_skipped(const char* s, size_t n) {
    return n == 0 || s[0] == '#';
}

// Split one significant line into key and value under the lexical rules of
// section 2.1.1: a key, a colon, one space, a non-empty value, no trailing
// whitespace.
static bool split_line(P* p, const char* s, size_t n, size_t number, Line* out) {
    char last = s[n - 1];
    if(last == ' ' || last == '\t' || last == '\r') {
        return fail(p, number, "trailing whitespace");
    }
    const char* sep = NULL;
    for(size_t i = 0; i + 1 < n; i++) {
        if(s[i] == ':' && s[i + 1] == ' ') {
            sep = s + i;
            break;
        }
    }
    if(!sep) return fail(p, number, "expected 'Key: Value' with a non-empty value");
    size_t key_len = (size_t)(sep - s);
    if(key_len == 0) return fail(p, number, "empty key");
    if(s[0] == ' ' || s[0] == '\t' || s[key_len - 1] == ' ' || s[key_len - 1] == '\t') {
        return fail(p, number, "malformed key");
    }
    out->key = s;
    out->key_len = key_len;
    out->value = sep + 2;
    out->value_len = n - key_len - 2;
    out->number = number;
    if(out->value_len == 0) return fail(p, number, "empty value");
    return true;
}

// Validate every line once, count the significant ones, and check that the two
// header keys lead (section 2.1.3).
static bool prescan(P* p) {
    size_t pos = 0;
    size_t number = 0;
    Line first = {0}, second = {0};
    while(true) {
        const char* s;
        size_t n;
        if(!raw_line(p, &pos, &s, &n)) break;
        number++;
        if(line_is_skipped(s, n)) continue;
        Line l;
        if(!split_line(p, s, n, number, &l)) return false;
        p->total++;
        if(p->total == 1) first = l;
        if(p->total == 2) second = l;
    }
    if(p->total < 2) return fail(p, 0, "Filetype and Version shall come first");
    if(first.key_len != 8 || memcmp(first.key, "Filetype", 8) != 0 || second.key_len != 7 ||
       memcmp(second.key, "Version", 7) != 0) {
        return fail(p, first.number, "Filetype and Version shall come first");
    }
    return true;
}

// Peek at the next significant line. Grammar rules request keys in wire order,
// so valid documents are consumed in one forward pass rather than rescanned
// from byte zero for every field.
static bool peek_line(P* p, Line* out) {
    if(p->has_lookahead) {
        *out = p->lookahead;
        return true;
    }
    size_t pos = p->cursor_pos;
    size_t number = p->cursor_line;
    while(true) {
        const char* s;
        size_t n;
        if(!raw_line(p, &pos, &s, &n)) return false;
        number++;
        if(line_is_skipped(s, n)) continue;
        if(!split_line(p, s, n, number, &p->lookahead)) return false;
        p->lookahead_end = pos;
        p->has_lookahead = true;
        *out = p->lookahead;
        return true;
    }
}

// Claim the next line when it carries `key`. Optional keys can therefore be
// tested without consuming the following field.
static bool lookup(P* p, const char* key, Line* out, bool claim) {
    if(p->failed) return false;
    Line line;
    if(!peek_line(p, &line)) return false;
    size_t key_len = strlen(key);
    if(line.key_len != key_len || memcmp(line.key, key, key_len) != 0) {
        if(p->last_key && line.key_len == p->last_key_len &&
           memcmp(line.key, p->last_key, line.key_len) == 0) {
            return fail(p, line.number, "duplicate key %.*s", (int)line.key_len, line.key);
        }
        return false;
    }
    *out = line;
    if(claim) {
        p->cursor_pos = p->lookahead_end;
        p->cursor_line = line.number;
        p->has_lookahead = false;
        p->last_key = line.key;
        p->last_key_len = line.key_len;
        p->consumed++;
    }
    return true;
}

static bool required(P* p, const char* key, Line* out) {
    if(lookup(p, key, out, true)) return true;
    if(p->failed) return false;
    Line next;
    if(peek_line(p, &next)) {
        return fail(
            p,
            next.number,
            "missing required key %s; next key is unrecognised or out of order",
            key);
    }
    return fail(p, 0, "missing required key %s", key);
}

static bool opt_preferred_auth_command(P* p, const char* key, bool* present, uint8_t* command) {
    *present = false;
    Line line;
    if(!lookup(p, key, &line, true)) return !p->failed;
    for(size_t i = 0; i < sizeof(AUTH_COMMAND_BITS); i++) {
        size_t len = strlen(AUTH_COMMAND_NAMES[i]);
        if(line.value_len == len && memcmp(line.value, AUTH_COMMAND_NAMES[i], len) == 0) {
            *present = true;
            *command = AUTH_COMMAND_BITS[i];
            return true;
        }
    }
    return fail(p, line.number, "%s: expected one authentication command", key);
}

// Octet count a hex value encodes, validating rule 3: uppercase pairs joined by
// single spaces, at least one octet.
static bool hex_count(P* p, const Line* l, const char* key, size_t* count) {
    if(l->value_len < 2 || (l->value_len + 1) % 3 != 0) {
        return fail(p, l->number, "%s: not uppercase hex octet pairs", key);
    }
    size_t n = (l->value_len + 1) / 3;
    for(size_t i = 0; i < n; i++) {
        const char* pair = l->value + i * 3;
        for(size_t j = 0; j < 2; j++) {
            char ch = pair[j];
            bool ok = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F');
            if(!ok) return fail(p, l->number, "%s: not uppercase hex octet pairs", key);
        }
        if(i + 1 < n && pair[2] != ' ') {
            return fail(p, l->number, "%s: not uppercase hex octet pairs", key);
        }
    }
    *count = n;
    return true;
}

static uint8_t hex_nibble(char ch) {
    return (uint8_t)(ch <= '9' ? ch - '0' : ch - 'A' + 10);
}

static void hex_decode(const Line* l, uint8_t* out, size_t count) {
    for(size_t i = 0; i < count; i++) {
        const char* pair = l->value + i * 3;
        out[i] = (uint8_t)((hex_nibble(pair[0]) << 4) | hex_nibble(pair[1]));
    }
}

// A required byte string of exactly `want` octets.
static bool req_hex(P* p, const char* key, uint8_t* out, size_t want) {
    Line l;
    if(!required(p, key, &l)) return false;
    size_t n = 0;
    if(!hex_count(p, &l, key, &n)) return false;
    if(n != want) {
        return fail(
            p, l.number, "%s: expected %u octets, found %u", key, (unsigned)want, (unsigned)n);
    }
    hex_decode(&l, out, n);
    return true;
}

// An optional byte string of 1 to `max` octets. `cap` is what this build can
// hold, which is a capacity failure rather than a malformed one.
static bool opt_hex(
    P* p,
    const char* key,
    uint8_t* out,
    size_t cap,
    size_t max,
    size_t* len,
    bool* present) {
    *present = false;
    *len = 0;
    Line l;
    if(!lookup(p, key, &l, true)) return !p->failed;
    size_t n = 0;
    if(!hex_count(p, &l, key, &n)) return false;
    if(n > max)
        return fail(p, l.number, "%s: length %u above %u", key, (unsigned)n, (unsigned)max);
    if(n > cap) {
        return fail_class(
            p, DfcTextCapacity, l.number, "%s: %u octets exceed this build", key, (unsigned)n);
    }
    hex_decode(&l, out, n);
    *len = n;
    *present = true;
    return true;
}

static bool opt_hex_exact(P* p, const char* key, uint8_t* out, size_t want, bool* present) {
    *present = false;
    Line l;
    if(!lookup(p, key, &l, true)) return !p->failed;
    size_t n = 0;
    if(!hex_count(p, &l, key, &n)) return false;
    if(n != want) {
        return fail(
            p, l.number, "%s: expected %u octets, found %u", key, (unsigned)want, (unsigned)n);
    }
    hex_decode(&l, out, n);
    *present = true;
    return true;
}

// Decimal integer of rule 4. `allow_sign` selects the signed form used by the
// three value-file limits.
static bool parse_decimal(P* p, const Line* l, const char* key, bool allow_sign, int64_t* out) {
    const char* v = l->value;
    size_t n = l->value_len;
    bool negative = false;
    if(allow_sign && n > 1 && v[0] == '-') {
        negative = true;
        v++;
        n--;
    }
    if(n == 0 || n > 10) return fail(p, l->number, "%s: not an integer", key);
    if(v[0] == '0' && n > 1) return fail(p, l->number, "%s: not an integer", key);
    uint64_t acc = 0;
    for(size_t i = 0; i < n; i++) {
        if(v[i] < '0' || v[i] > '9') return fail(p, l->number, "%s: not an integer", key);
        acc = acc * 10 + (uint64_t)(v[i] - '0');
    }
    if(acc > 4294967295u) return fail(p, l->number, "%s: out of range", key);
    *out = negative ? -(int64_t)acc : (int64_t)acc;
    return true;
}

static bool req_uint(P* p, const char* key, uint32_t lo, uint32_t hi, uint32_t* out) {
    Line l;
    if(!required(p, key, &l)) return false;
    int64_t v = 0;
    if(!parse_decimal(p, &l, key, false, &v)) return false;
    if((uint64_t)v < lo || (uint64_t)v > hi) {
        return fail(
            p,
            l.number,
            "%s: %lu outside %lu..%lu",
            key,
            (unsigned long)v,
            (unsigned long)lo,
            (unsigned long)hi);
    }
    *out = (uint32_t)v;
    return true;
}

static bool req_int32(P* p, const char* key, int32_t* out) {
    Line l;
    if(!required(p, key, &l)) return false;
    int64_t v = 0;
    if(!parse_decimal(p, &l, key, true, &v)) return false;
    if(v < INT32_MIN || v > INT32_MAX) return fail(p, l.number, "%s: outside Int32", key);
    *out = (int32_t)v;
    return true;
}

static bool opt_nonnegative_int32(P* p, const char* key, int32_t* out) {
    *out = 0;
    Line l;
    if(!lookup(p, key, &l, true)) return !p->failed;
    int64_t value = 0;
    if(!parse_decimal(p, &l, key, false, &value)) return false;
    if(value < 0 || value > INT32_MAX) {
        return fail(p, l.number, "%s: outside 0..Int32Max", key);
    }
    *out = (int32_t)value;
    return true;
}

static bool req_bool(P* p, const char* key, bool* out) {
    Line l = {0};
    if(!required(p, key, &l)) return false;
    if(l.value_len != 1 || (l.value[0] != '0' && l.value[0] != '1')) {
        return fail(p, l.number, "%s: boolean shall be 0 or 1", key);
    }
    *out = l.value[0] == '1';
    return true;
}

// An optional boolean defaulting to 0. An explicit 0 is accepted as
// non-canonical input; the writer omits it (rule 5).
static bool opt_bool(P* p, const char* key, bool* out) {
    *out = false;
    Line l = {0};
    if(!lookup(p, key, &l, true)) return !p->failed;
    if(l.value_len != 1 || (l.value[0] != '0' && l.value[0] != '1')) {
        return fail(p, l.number, "%s: boolean shall be 0 or 1", key);
    }
    *out = l.value[0] == '1';
    return true;
}

#if DFC_ENABLE_SDM
// Reads an optional decimal value. Sets *present to false when the key is absent.
static bool
    opt_uint(P* p, const char* key, uint32_t lo, uint32_t hi, uint32_t* out, bool* present) {
    *present = false;
    Line l;
    if(!lookup(p, key, &l, true)) return !p->failed;
    int64_t value = 0;
    if(!parse_decimal(p, &l, key, false, &value)) return false;
    if((uint64_t)value < lo || (uint64_t)value > hi) {
        return fail(
            p,
            l.number,
            "%s: %lu outside %lu..%lu",
            key,
            (unsigned long)value,
            (unsigned long)lo,
            (unsigned long)hi);
    }
    *out = (uint32_t)value;
    *present = true;
    return true;
}
#endif

// A bare enumeration token, matched case-sensitively (rule 6).
static bool
    req_token(P* p, const char* key, const char* const* names, size_t count, size_t* index) {
    Line l;
    if(!required(p, key, &l)) return false;
    for(size_t i = 0; i < count; i++) {
        size_t n = strlen(names[i]);
        if(l.value_len == n && memcmp(l.value, names[i], n) == 0) {
            *index = i;
            return true;
        }
    }
    return fail(p, l.number, "%s: unknown token", key);
}

static bool opt_iso_file_id(P* p, const char* key, bool* present, uint16_t* value) {
    *present = false;
    Line l;
    if(!lookup(p, key, &l, true)) return !p->failed;
    size_t n = 0;
    if(!hex_count(p, &l, key, &n)) return false;
    if(n != 2) return fail(p, l.number, "%s: expected 2 octets", key);
    uint8_t raw[2];
    hex_decode(&l, raw, 2);
    uint16_t v = (uint16_t)((raw[0] << 8) | raw[1]);
    if(dfc_iso_file_id_is_reserved(v)) return fail(p, l.number, "%s: %04X is reserved", key, v);
    *present = true;
    *value = v;
    return true;
}

// `app` selects an application, NULL the PICC record.
static bool parse_keys(P* p, const char* prefix, uint8_t key_settings_2, DfcApplication* app) {
    char key[KEY_MAX];
    uint8_t* versions = app ? app->key_versions : p->c->picc_key_versions;
    snprintf(key, sizeof(key), "%s Key Count", prefix);
    uint32_t count = 0;
    if(!req_uint(p, key, 0, 14, &count)) return false;
    if(count > DFC_MAX_KEYS) {
        return fail_class(
            p, DfcTextCapacity, 0, "%s: %u keys exceed this build", key, (unsigned)count);
    }
    size_t want = dfc_credential_key_length(key_settings_2);
    if(!dfc_credential_keys_resize(p->c, app, (size_t)count, want)) {
        return fail_class(p, DfcTextCapacity, 0, "%s: %u keys do not fit", key, (unsigned)count);
    }
    for(size_t i = 0; i < count; i++) {
        uint8_t* dst = dfc_credential_key(p->c, app, i);
        if(!dst) return fail_class(p, DfcTextCapacity, 0, "%s: no room for keys", prefix);
        snprintf(key, sizeof(key), "%s Key %02X", prefix, (unsigned)i);
        if(!req_hex(p, key, dst, want)) return false;
        snprintf(key, sizeof(key), "%s Key %02X Version", prefix, (unsigned)i);
        if(!req_hex(p, key, &versions[i], 1)) return false;
    }
    return true;
}

static bool parse_sdm(P* p, const char* prefix, DfcFile* f) {
    char key[KEY_MAX];
    snprintf(key, sizeof(key), "%s SDM Options", prefix);
    Line probe;
    if(!lookup(p, key, &probe, false)) return !p->failed;
#if DFC_ENABLE_SDM
    if(!req_hex(p, key, &f->state.data.sdm.options, 1)) return false;
    snprintf(key, sizeof(key), "%s SDM Access Rights", prefix);
    uint8_t rights[2];
    if(!req_hex(p, key, rights, 2)) return false;
    f->state.data.sdm.access_rights = (uint16_t)((rights[0] << 8) | rights[1]);

    static const char* const OFFSETS[] = {
        "UID Offset",
        "Counter Offset",
        "PICC Data Offset",
        "MAC Input Offset",
        "MAC Offset",
        "Encrypted File Offset"};
    static const DfcSdmFlag flags[] = {
        DfcSdmFlagUidOffset,
        DfcSdmFlagCounterOffset,
        DfcSdmFlagPiccDataOffset,
        DfcSdmFlagMacInputOffset,
        DfcSdmFlagMacOffset,
        DfcSdmFlagEncryptedFile};
    uint32_t* values[] = {
        &f->state.data.sdm.uid_offset,
        &f->state.data.sdm.counter_offset,
        &f->state.data.sdm.picc_data_offset,
        &f->state.data.sdm.mac_input_offset,
        &f->state.data.sdm.mac_offset,
        &f->state.data.sdm.encrypted_file_offset};
    for(size_t i = 0; i < 6; i++) {
        snprintf(key, sizeof(key), "%s SDM %s", prefix, OFFSETS[i]);
        bool present = false;
        if(!opt_uint(p, key, 0, U24_MAX, values[i], &present)) return false;
        dfc_file_sdm_flag_set(f, flags[i], present);
    }

    bool has_length = false;
    uint32_t length = 0;
    snprintf(key, sizeof(key), "%s SDM Encrypted File Length", prefix);
    if(!opt_uint(p, key, 0, U24_MAX, &length, &has_length)) return false;
    // The encrypted offset and its length occur together.
    if(has_length != dfc_file_sdm_flag_is_set(f, DfcSdmFlagEncryptedFile)) {
        return fail(p, 0, "%s: SDM encrypted offset and length occur together", prefix);
    }
    f->state.data.sdm.encrypted_file_length = length;

    snprintf(key, sizeof(key), "%s SDM Counter Limit", prefix);
    bool has_limit = false;
    if(!opt_uint(p, key, 0, U24_MAX, &f->state.data.sdm.counter_limit, &has_limit)) {
        return false;
    }
    dfc_file_sdm_flag_set(f, DfcSdmFlagCounterLimit, has_limit);
    snprintf(key, sizeof(key), "%s SDM Read Counter", prefix);
    if(!req_uint(p, key, 0, U24_MAX, &f->state.data.sdm_read_counter)) return false;
    dfc_file_sdm_flag_set(f, DfcSdmFlagEnabled, true);
    return true;
#else
    (void)f;
    // The field is recognized but the build omits the feature.
    return fail_class(p, DfcTextUnsupported, 0, "%s SDM Options: not in this build", prefix);
#endif
}

static bool parse_data_contents(P* p, const char* prefix, DfcFile* f) {
    char key[KEY_MAX];
    snprintf(key, sizeof(key), "%s Size", prefix);
    uint32_t size = 0;
    if(!req_uint(p, key, 1, U24_MAX, &size)) return false;
    if(!dfc_file_set_declared_size(f, size)) return false;

    snprintf(key, sizeof(key), "%s Data", prefix);
    Line l;
    size_t n = 0;
    bool has_data = lookup(p, key, &l, true);
    if(p->failed) return false;
    if(has_data) {
        if(!hex_count(p, &l, key, &n)) return false;
        if(n > size) return fail(p, l.number, "%s: Data longer than Size", prefix);
        if(!dfc_file_resize(p->c, f, n)) {
            return fail_class(
                p, DfcTextCapacity, l.number, "%s: %u octets do not fit", prefix, (unsigned)n);
        }
        uint8_t* dst = dfc_file_data(p->c, f);
        if(!dst) return fail_class(p, DfcTextCapacity, l.number, "%s: no room for Data", prefix);
        hex_decode(&l, dst, n);
    }

    snprintf(key, sizeof(key), "%s Data Complete", prefix);
    if(!req_bool(p, key, &f->contents_complete)) return false;
    if(f->contents_complete && f->data_len != size) {
        return fail(p, 0, "%s: Data Complete is 1 but Data length is not Size", prefix);
    }
    return parse_sdm(p, prefix, f);
}

static bool parse_value_contents(P* p, const char* prefix, DfcFile* f) {
    char key[KEY_MAX];
    snprintf(key, sizeof(key), "%s Value Lower Limit", prefix);
    if(!req_int32(p, key, &f->state.value.lower_limit)) return false;
    snprintf(key, sizeof(key), "%s Value Upper Limit", prefix);
    if(!req_int32(p, key, &f->state.value.upper_limit)) return false;
    snprintf(key, sizeof(key), "%s Value", prefix);
    if(!req_int32(p, key, &f->state.value.current)) return false;
    if(f->state.value.lower_limit > f->state.value.current ||
       f->state.value.current > f->state.value.upper_limit) {
        return fail(p, 0, "%s: Value outside its own limits", prefix);
    }
    snprintf(key, sizeof(key), "%s Limited Credit", prefix);
    if(!req_hex(p, key, &f->state.value.limited_credit, 1)) return false;
    snprintf(key, sizeof(key), "%s Limited Credit Value", prefix);
    return opt_nonnegative_int32(p, key, &f->state.value.limited_credit_value);
}

static bool parse_record_contents(P* p, const char* prefix, DfcFile* f) {
    char key[KEY_MAX];
    uint32_t rsize = 0, rmax = 0, rcount = 0;
    snprintf(key, sizeof(key), "%s Record Size", prefix);
    if(!req_uint(p, key, 1, U24_MAX, &rsize)) return false;
    snprintf(key, sizeof(key), "%s Max Records", prefix);
    if(!req_uint(p, key, 1, U24_MAX, &rmax)) return false;
    snprintf(key, sizeof(key), "%s Record Count", prefix);
    if(!req_uint(p, key, 0, U24_MAX, &rcount)) return false;
    if(rcount > rmax) return fail(p, 0, "%s: Max Records below Record Count", prefix);
    if(f->type == TYPE_CYCLIC && rmax < 2) {
        return fail(p, 0, "%s: cyclic Max Records below 2", prefix);
    }
    f->state.record.size = (uint32_t)rsize;
    f->state.record.max_records = (uint32_t)rmax;
    f->state.record.count = (uint32_t)rcount;

    // Count the contiguous record lines without moving the parser. The pool
    // slice is sized once, then the same short span is consumed and decoded.
    size_t stored = 0;
    size_t pos = p->cursor_pos;
    size_t number = p->cursor_line;
    while(true) {
        snprintf(key, sizeof(key), "%s Record %02X", prefix, (unsigned)stored);
        const char* s;
        size_t len;
        Line l;
        do {
            if(!raw_line(p, &pos, &s, &len)) goto records_counted;
            number++;
        } while(line_is_skipped(s, len));
        if(!split_line(p, s, len, number, &l)) return false;
        size_t key_len = strlen(key);
        if(l.key_len != key_len || memcmp(l.key, key, key_len) != 0) break;
        stored++;
    }
records_counted:
    if(p->failed) return false;
    if(stored > rcount) return fail(p, 0, "%s: more records than Record Count", prefix);

    uint8_t* dst = NULL;
    if(stored > 0) {
        if(!dfc_file_resize(p->c, f, stored * (size_t)rsize)) {
            return fail_class(p, DfcTextCapacity, 0, "%s: records do not fit", prefix);
        }
        dst = dfc_file_data(p->c, f);
        if(!dst) return fail_class(p, DfcTextCapacity, 0, "%s: no room for records", prefix);
        for(size_t i = 0; i < stored; i++) {
            snprintf(key, sizeof(key), "%s Record %02X", prefix, (unsigned)i);
            Line l;
            if(!lookup(p, key, &l, true)) return fail(p, 0, "%s: record vanished", prefix);
            size_t n = 0;
            if(!hex_count(p, &l, key, &n)) return false;
            if(n != rsize) return fail(p, l.number, "%s: length is not Record Size", key);
            hex_decode(&l, dst + i * (size_t)rsize, (size_t)rsize);
        }
    }

    snprintf(key, sizeof(key), "%s Record Complete", prefix);
    if(!req_bool(p, key, &f->contents_complete)) return false;
    if(f->contents_complete && stored != rcount) {
        return fail(p, 0, "%s: Record Complete is 1 but records are missing", prefix);
    }

    return true;
}

static bool parse_transaction_mac_contents(P* p, const char* prefix, DfcFile* f) {
#if DFC_ENABLE_TRANSACTION_MAC
    char key[KEY_MAX];
    snprintf(key, sizeof(key), "%s Transaction Counter", prefix);
    if(!req_uint(p, key, 0, UINT32_MAX, &f->state.transaction_mac.counter)) return false;
    snprintf(key, sizeof(key), "%s Transaction MAC", prefix);
    if(!req_hex(p, key, f->state.transaction_mac.mac, sizeof(f->state.transaction_mac.mac))) {
        return false;
    }
    snprintf(key, sizeof(key), "%s Transaction Key Type", prefix);
    uint32_t key_type = 0;
    if(!req_uint(p, key, 0, 0xFF, &key_type)) return false;
    f->state.transaction_mac.key_type = (uint8_t)key_type;
    snprintf(key, sizeof(key), "%s Transaction Key Version", prefix);
    if(!req_hex(p, key, &f->state.transaction_mac.key_version, 1)) return false;
    snprintf(key, sizeof(key), "%s Transaction Key", prefix);
    if(!req_hex(p, key, f->state.transaction_mac.key, sizeof(f->state.transaction_mac.key)))
        return false;
    snprintf(key, sizeof(key), "%s Previous Reader ID", prefix);
    return req_hex(
        p,
        key,
        f->state.transaction_mac.previous_reader_id,
        sizeof(f->state.transaction_mac.previous_reader_id));
#else
    (void)f;
    // The field is recognized but the build omits the feature.
    return fail_class(
        p, DfcTextUnsupported, 0, "%s: transaction MAC is not in this build", prefix);
#endif
}

static bool parse_files(P* p, const char* parent, size_t owner) {
    char key[KEY_MAX];
    char prefix[PREFIX_MAX];
    snprintf(key, sizeof(key), "%s File Count", parent);
    uint32_t count = 0;
    if(!req_uint(p, key, 0, DFC_EV1_MAX_FILES_PER_APP, &count)) return false;

    for(size_t i = 0; i < count; i++) {
        snprintf(prefix, sizeof(prefix), "%s File %02X", parent, (unsigned)i);
        if(p->c->num_files >= DFC_MAX_FILES) {
            return fail_class(p, DfcTextCapacity, 0, "%s: file does not fit this build", prefix);
        }
        DfcFile* f = &p->c->files[p->c->num_files];
        memset(f, 0, sizeof(*f));
        f->app_index = (uint8_t)owner;
        f->data_offset = DFC_FILE_POOL_NONE;

        snprintf(key, sizeof(key), "%s Number", prefix);
        if(!req_hex(p, key, &f->number, 1)) return false;
        if(f->number > DFC_EV1_MAX_FILE_NUMBER) {
            return fail(p, 0, "%s Number: %02X above 1F", prefix, f->number);
        }
        for(size_t j = 0; j < p->c->num_files; j++) {
            if(p->c->files[j].app_index == owner && p->c->files[j].number == f->number) {
                return fail(p, 0, "%s Number: duplicate %02X", prefix, f->number);
            }
        }

        snprintf(key, sizeof(key), "%s Type", prefix);
        size_t type_index = 0;
        if(!req_token(p, key, FILE_TYPE_NAMES, 6, &type_index)) return false;
        f->type = (uint8_t)type_index;

        snprintf(key, sizeof(key), "%s Communication Settings", prefix);
        if(!req_hex(p, key, &f->comm_settings, 1)) return false;

        snprintf(key, sizeof(key), "%s Access Rights", prefix);
        uint8_t rights[2];
        if(!req_hex(p, key, rights, 2)) return false;
        f->access_rights = (uint16_t)((rights[0] << 8) | rights[1]);

        snprintf(key, sizeof(key), "%s ISO File ID", prefix);
        if(!opt_iso_file_id(p, key, &f->has_iso_file_id, &f->iso_file_id)) return false;

        // The count moves before the contents so the pool slice belongs to a
        // file the model already owns.
        p->c->num_files++;

        if(type_is_data(f->type)) {
            if(!parse_data_contents(p, prefix, f)) return false;
        } else if(f->type == TYPE_VALUE) {
            if(!parse_value_contents(p, prefix, f)) return false;
        } else if(type_is_record(f->type)) {
            if(!parse_record_contents(p, prefix, f)) return false;
        } else if(f->type == TYPE_TMAC) {
            if(!parse_transaction_mac_contents(p, prefix, f)) return false;
        } else {
            return fail_class(p, DfcTextUnsupported, 0, "%s Type: unsupported", prefix);
        }
    }
    return true;
}

static bool parse_capability_data(P* p, const char* prefix, DfcApplication* app) {
    char key[KEY_MAX];
    snprintf(key, sizeof(key), "%s Capability Data", prefix);
    Line probe;
    if(!lookup(p, key, &probe, false)) return !p->failed;
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
    if(!req_hex(p, key, app->capability_data, sizeof(app->capability_data))) return false;
    app->has_capability_data = true;
    return true;
#else
    (void)app;
    // The field is recognized but the build omits the feature.
    return fail_class(p, DfcTextUnsupported, 0, "%s: not in this build", key);
#endif
}

static bool parse_delegated(P* p, const char* prefix, DfcApplication* app) {
    char key[KEY_MAX];
    snprintf(key, sizeof(key), "%s Delegated Slot Number", prefix);
    Line probe;
    if(!lookup(p, key, &probe, false)) return !p->failed;
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    uint32_t slot = 0, quota = 0, free_blocks = 0;
    if(!req_uint(p, key, 0, 0xFFFFu, &slot)) return false;
    snprintf(key, sizeof(key), "%s Delegated Slot Version", prefix);
    if(!req_hex(p, key, &app->delegated_slot_version, 1)) return false;
    snprintf(key, sizeof(key), "%s Delegated Quota Limit", prefix);
    if(!req_uint(p, key, 0, 0xFFFFu, &quota)) return false;
    snprintf(key, sizeof(key), "%s Delegated Free Blocks", prefix);
    if(!req_uint(p, key, 0, 0xFFFFu, &free_blocks)) return false;
    snprintf(key, sizeof(key), "%s Delegated Format Disabled", prefix);
    if(!opt_bool(p, key, &app->delegated_format_disabled)) return false;
    if(free_blocks > quota) {
        return fail(p, 0, "%s Delegated Free Blocks: exceeds the quota", prefix);
    }
    app->delegated_slot_number = (uint16_t)slot;
    app->delegated_quota_limit = (uint16_t)quota;
    app->delegated_free_blocks = (uint16_t)free_blocks;
    app->delegated = true;
    return true;
#else
    (void)app;
    return fail_class(p, DfcTextUnsupported, 0, "%s: not in this build", key);
#endif
}

static bool parse_key_sets(P* p, const char* prefix, DfcApplication* app) {
#if DFC_ENABLE_KEY_SETS
    char key[KEY_MAX];
    char name[PREFIX_MAX];
    uint32_t key_count = 0, max_size = 0, count = 0;
    snprintf(key, sizeof(key), "%s Key Set Key Count", prefix);
    if(!req_uint(p, key, 1, DFC_MAX_KEYS, &key_count)) return false;
    snprintf(key, sizeof(key), "%s Key Set Maximum Key Size", prefix);
    if(!req_uint(p, key, 16, 24, &max_size)) return false;
    if(max_size != 16 && max_size != 24) {
        return fail(p, 0, "%s: maximum key size shall be 16 or 24", key);
    }
    snprintf(key, sizeof(key), "%s Key Set Settings", prefix);
    uint8_t settings = 0;
    if(!req_hex(p, key, &settings, 1)) return false;
    snprintf(key, sizeof(key), "%s Key Set Count", prefix);
    if(!req_uint(p, key, DFC_KEY_SET_MINIMUM_COUNT, DFC_MAX_KEY_SETS, &count)) return false;
    if(!dfc_credential_key_sets_resize(p->c, app, count, key_count, app->key_len, max_size)) {
        return fail_class(p, DfcTextCapacity, 0, "%s: key sets do not fit this build", prefix);
    }
    app->key_set_settings = settings;

    for(size_t set = 0; set < count; set++) {
        uint8_t* set_version = dfc_credential_key_set_version(p->c, app, set);
        uint8_t* set_type = dfc_credential_key_set_type(p->c, app, set);
        if(!set_version || !set_type) {
            return fail_class(
                p, DfcTextCapacity, 0, "%s: metadata does not fit this build", prefix);
        }
        snprintf(name, sizeof(name), "%s Key Set %02X", prefix, (unsigned)set);
        snprintf(key, sizeof(key), "%s Version", name);
        if(!req_hex(p, key, set_version, 1)) return false;
        snprintf(key, sizeof(key), "%s Type", name);
        size_t type_index = 0;
        if(!req_token(p, key, AUTH_NAMES, 3, &type_index)) return false;
        *set_type = (uint8_t)type_index;
        size_t required = dfc_key_set_type_length((uint8_t)type_index);
        if(required > max_size) {
            return fail(p, 0, "%s Type: key type exceeds the maximum key size", name);
        }
        snprintf(key, sizeof(key), "%s Initialized", name);
        bool initialized;
        if(!req_bool(p, key, &initialized) ||
           !dfc_credential_key_set_set_initialized(p->c, app, set, initialized))
            return false;
        uint32_t set_keys = 0;
        snprintf(key, sizeof(key), "%s Key Count", name);
        if(!req_uint(p, key, 0, key_count, &set_keys)) return false;
        for(size_t slot = 0; slot < set_keys; slot++) {
            uint8_t* value = dfc_credential_key_in_set(p->c, app, set, slot);
            uint8_t* version = dfc_credential_key_version_in_set(p->c, app, set, slot);
            if(!value || !version) {
                return fail_class(p, DfcTextCapacity, 0, "%s: key does not fit this build", name);
            }
            snprintf(key, sizeof(key), "%s Key %02X", name, (unsigned)slot);
            if(!req_hex(p, key, value, required)) return false;
            snprintf(key, sizeof(key), "%s Key %02X Version", name, (unsigned)slot);
            if(!req_hex(p, key, version, 1)) return false;
        }
    }
    if(!dfc_credential_key_set_is_initialized(p->c, app, 0)) {
        return fail(p, 0, "%s Key Set 00: set zero shall be initialized", prefix);
    }
    return true;
#else
    (void)app;
    // The field is recognized but the build omits the feature.
    return fail_class(p, DfcTextUnsupported, 0, "%s Key Set Count: not in this build", prefix);
#endif
}

static bool parse_applications(P* p) {
    char key[KEY_MAX];
    char prefix[PARENT_MAX];
    uint32_t count = 0;
    if(!req_uint(p, "Application Count", 0, 0xFF, &count)) return false;
    if(count > DFC_MAX_APPS) {
        return fail_class(
            p, DfcTextCapacity, 0, "Application Count: %u exceeds this build", (unsigned)count);
    }

    for(size_t i = 0; i < count; i++) {
        snprintf(prefix, sizeof(prefix), "Application %02X", (unsigned)(uint8_t)i);
        DfcApplication* app = &p->c->apps[i];
        dfc_credential_reset_application(app);

        snprintf(key, sizeof(key), "%s AID", prefix);
        if(!req_hex(p, key, app->aid, 3)) return false;
        for(size_t j = 0; j < i; j++) {
            if(memcmp(p->c->apps[j].aid, app->aid, 3) == 0) {
                return fail(p, 0, "%s AID: duplicate", prefix);
            }
        }

        snprintf(key, sizeof(key), "%s ISO File ID", prefix);
        if(!opt_iso_file_id(p, key, &app->has_iso_file_id, &app->iso_file_id)) return false;

        snprintf(key, sizeof(key), "%s DF Name", prefix);
        bool has_df = false;
        size_t iso_aid_len = 0;
        if(!opt_hex(p, key, app->iso_aid, sizeof(app->iso_aid), 16, &iso_aid_len, &has_df)) {
            return false;
        }
        app->iso_aid_len = (uint8_t)iso_aid_len;

        snprintf(key, sizeof(key), "%s Key Settings 1", prefix);
        if(!req_hex(p, key, &app->key_settings_1, 1)) return false;
        snprintf(key, sizeof(key), "%s Key Settings 2", prefix);
        if(!req_hex(p, key, &app->key_settings_2, 1)) return false;

        if(p->format_version >= 6) {
            snprintf(key, sizeof(key), "%s Authentication Commands", prefix);
            if(!req_auth_commands(p, key, &app->auth_commands)) return false;
            app->has_auth_commands = true;
            snprintf(key, sizeof(key), "%s Preferred Authentication Command", prefix);
            if(!opt_preferred_auth_command(
                   p, key, &app->has_preferred_auth_command, &app->preferred_auth_command))
                return false;
        } else {
            snprintf(key, sizeof(key), "%s Authentication Mode", prefix);
            size_t auth = 0;
            if(!req_token(p, key, AUTH_NAMES, 3, &auth)) return false;
            app->auth_command = auth_command_for(auth);
            app->preferred_auth_command = AUTH_COMMAND_BITS[auth];
            app->has_preferred_auth_command = true;
        }

        char selector[KEY_MAX];
        snprintf(selector, sizeof(selector), "%s Key Set Key Count", prefix);
        Line probe;
        // The key-set block replaces the ordinary key list.
        if(lookup(p, selector, &probe, false)) {
            app->key_len = dfc_credential_key_length(app->key_settings_2);
            if(!parse_key_sets(p, prefix, app)) return false;
        } else {
            if(p->failed) return false;
            if(!parse_keys(p, prefix, app->key_settings_2, app)) return false;
        }

        // Files reference their owner by index, so the application has to be
        // part of the model before they are parsed.
        p->c->num_apps = (uint8_t)(i + 1);
        if(!parse_files(p, prefix, i)) return false;
        if(!parse_capability_data(p, prefix, app)) return false;
        if(!parse_delegated(p, prefix, app)) return false;
        if(p->format_version >= 6) {
            size_t sm_len = 0;
            snprintf(key, sizeof(key), "%s SM Disable", prefix);
            if(!opt_hex(p, key, &app->sm_disable, 1, 1, &sm_len, &app->has_sm_disable)) {
                return false;
            }
        }
    }
    return true;
}

static bool parse_static_signature(P* p) {
    DfcCredential* c = p->c;
    (void)c;
    Line probe;
    if(!lookup(p, "Card Static Signature", &probe, false)) return !p->failed;
#if DFC_ENABLE_STATIC_SIGNATURE
    if(!req_hex(
           p,
           "Card Static Signature",
           c->picc_static_signature,
           sizeof(c->picc_static_signature))) {
        return false;
    }
    c->picc_has_static_signature = true;
    return true;
#else
    // The field is recognized but the build omits the feature.
    return fail_class(p, DfcTextUnsupported, 0, "Card Static Signature: not in this build");
#endif
}

static bool parse_ev2_capabilities(P* p) {
    DfcCredential* c = p->c;
    (void)c;
    Line probe;
    if(!lookup(p, "PICC EV2 Card Capabilities", &probe, false)) return !p->failed;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(!req_hex(
           p,
           "PICC EV2 Card Capabilities",
           c->picc_ev2_capabilities,
           sizeof(c->picc_ev2_capabilities))) {
        return false;
    }
    c->picc_has_ev2_capabilities = true;
    return true;
#else
    // The field is recognized but the build omits the feature.
    return fail_class(p, DfcTextUnsupported, 0, "PICC EV2 Card Capabilities: not in this build");
#endif
}

static bool parse_proximity(P* p) {
    DfcCredential* c = p->c;
    (void)c;
    Line probe;
    if(!lookup(p, "PICC Proximity Key", &probe, false)) return !p->failed;
#if DFC_ENABLE_PROXIMITY_CHECK
    if(!req_hex(p, "PICC Proximity Key", c->picc_proximity_key, sizeof(c->picc_proximity_key))) {
        return false;
    }
    if(!req_hex(p, "PICC Proximity Option", &c->picc_proximity_option, 1)) return false;
    uint32_t published = 0;
    if(!req_uint(p, "PICC Proximity Published Response Time", 0, 0xFFFFu, &published))
        return false;
    c->picc_proximity_published_response_time = (uint16_t)published;
    size_t len = 0;
    if(!opt_hex(
           p,
           "PICC Proximity Bitrate",
           &c->picc_proximity_bitrate,
           1,
           1,
           &len,
           &c->picc_has_proximity_bitrate)) {
        return false;
    }
    c->picc_has_proximity_key = true;
    return true;
#else
    return fail_class(p, DfcTextUnsupported, 0, "PICC Proximity Key: not in this build");
#endif
}

static bool parse_virtual_card(P* p) {
    DfcCredential* c = p->c;
    (void)c;
    Line probe;
    if(!lookup(p, "PICC Virtual Card Installation ID", &probe, false)) return !p->failed;
#if DFC_ENABLE_VIRTUAL_CARD
    bool present = false;
    size_t installation_id_len = 0;
    if(!opt_hex(
           p,
           "PICC Virtual Card Installation ID",
           c->virtual_card_installation_id,
           sizeof(c->virtual_card_installation_id),
           DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH,
           &installation_id_len,
           &present)) {
        return false;
    }
    if(installation_id_len == 0) {
        return fail(p, 0, "PICC Virtual Card Installation ID: at least one octet");
    }
    c->virtual_card_installation_id_len = (uint8_t)installation_id_len;
    if(!req_hex(p, "PICC Virtual Card Information", &c->virtual_card_information, 1)) return false;
    if(!req_hex(
           p,
           "PICC Virtual Card Capabilities",
           c->virtual_card_capabilities,
           sizeof(c->virtual_card_capabilities))) {
        return false;
    }
    size_t virtual_uid_len = 0;
    if(!opt_hex(
           p,
           "PICC Virtual Card UID",
           c->virtual_card_uid,
           sizeof(c->virtual_card_uid),
           DFC_VIRTUAL_CARD_UID_MAX_LENGTH,
           &virtual_uid_len,
           &present)) {
        return false;
    }
    if(!dfc_uid_length_is_valid(virtual_uid_len)) {
        return fail(p, 0, "PICC Virtual Card UID: length shall be 4, 7, or 10");
    }
    c->virtual_card_uid_len = (uint8_t)virtual_uid_len;
    if(!req_hex(
           p,
           "PICC Virtual Card Select MAC Key",
           c->virtual_card_select_mac_key,
           sizeof(c->virtual_card_select_mac_key))) {
        return false;
    }
    if(!req_hex(
           p,
           "PICC Virtual Card Select Encryption Key",
           c->virtual_card_select_encryption_key,
           sizeof(c->virtual_card_select_encryption_key))) {
        return false;
    }
    // Both flags are required, so an absent one is malformed.
    if(!req_bool(
           p,
           "PICC Virtual Card Authentication Mandatory",
           &c->virtual_card_authentication_mandatory)) {
        return false;
    }
    if(!req_bool(
           p, "PICC Virtual Card Proximity Mandatory", &c->virtual_card_proximity_mandatory)) {
        return false;
    }
    c->virtual_card_configured = true;
    return true;
#else
    return fail_class(
        p, DfcTextUnsupported, 0, "PICC Virtual Card Installation ID: not in this build");
#endif
}

static bool parse_dam_keys(P* p) {
    DfcCredential* c = p->c;
    (void)c;
    Line probe;
    if(!lookup(p, "PICC DAM Authentication Key", &probe, false)) return !p->failed;
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    if(!req_hex(
           p, "PICC DAM Authentication Key", c->picc_dam_auth_key, sizeof(c->picc_dam_auth_key))) {
        return false;
    }
    if(!req_hex(p, "PICC DAM MAC Key", c->picc_dam_mac_key, sizeof(c->picc_dam_mac_key))) {
        return false;
    }
    if(!req_hex(
           p,
           "PICC DAM Encryption Key",
           c->picc_dam_encryption_key,
           sizeof(c->picc_dam_encryption_key))) {
        return false;
    }
    c->picc_has_dam_keys = true;
    return true;
#else
    return fail_class(p, DfcTextUnsupported, 0, "PICC DAM Authentication Key: not in this build");
#endif
}

static bool parse_picc(P* p) {
    DfcCredential* c = p->c;
    if(!req_hex(p, "PICC Key Settings 1", &c->picc_key_settings_1, 1)) return false;
    if(!req_hex(p, "PICC Key Settings 2", &c->picc_key_settings_2, 1)) return false;
    if(p->format_version >= 6) {
        if(!req_auth_commands(p, "PICC Authentication Commands", &c->picc_auth_commands)) {
            return false;
        }
        c->picc_has_auth_commands = true;
        if(!opt_preferred_auth_command(
               p,
               "PICC Preferred Authentication Command",
               &c->picc_has_preferred_auth_command,
               &c->picc_preferred_auth_command))
            return false;
    } else {
        size_t auth = 0;
        if(!req_token(p, "PICC Authentication Mode", AUTH_NAMES, 3, &auth)) return false;
        c->picc_auth_command = auth_command_for(auth);
        c->picc_preferred_auth_command = AUTH_COMMAND_BITS[auth];
        c->picc_has_preferred_auth_command = true;
    }
    if(!parse_keys(p, "PICC", c->picc_key_settings_2, NULL)) return false;
    if(!opt_bool(p, "PICC Random ID", &c->picc_random_id)) return false;
    if(!opt_bool(p, "PICC Random ID Legacy Prefix", &c->picc_random_id_legacy)) return false;
    if(c->picc_random_id_legacy && !c->picc_random_id) {
        return fail(p, 0, "PICC Random ID Legacy Prefix requires PICC Random ID");
    }
    if(!opt_bool(p, "PICC Format Disabled", &c->picc_format_disabled)) return false;

    bool present = false;
    size_t ats_len = 0;
    if(!opt_hex(p, "PICC ATS", c->picc_ats, sizeof(c->picc_ats), U24_MAX, &ats_len, &present)) {
        return false;
    }
    c->picc_ats_len = (uint8_t)ats_len;
    size_t len = 0;
    if(!opt_hex(p, "PICC SAK", &c->picc_sak, 1, 1, &len, &c->picc_has_sak)) return false;
    if(!opt_hex(p, "PICC ATQA", c->picc_atqa, 2, 2, &len, &c->picc_has_atqa)) return false;
    if(c->picc_has_atqa && len != 2) return fail(p, 0, "PICC ATQA: expected 2 octets");
    if(!opt_hex(p, "PICC SM Disable", &c->picc_sm_disable, 1, 1, &len, &c->picc_has_sm_disable)) {
        return false;
    }
    if(!parse_ev2_capabilities(p)) return false;
    if(!parse_proximity(p)) return false;
    if(!parse_virtual_card(p)) return false;
    if(!parse_dam_keys(p)) return false;
    return true;
}

// Fills `credential` in place. Like the binary decoder, it is only reached
// through its public entry point, which owns the failure contract.
static DfcTextStatus parse_credential(
    DfcCredential* credential,
    const char* text,
    size_t len,
    DfcTextError* detail) {
    if(detail) {
        detail->line = 0;
        detail->message[0] = '\0';
    }
    if(!credential || !text) return DfcTextMalformed;
    if(len > DFC_TEXT_MAX_SIZE) {
        if(detail) {
            detail->line = 0;
            snprintf(detail->message, sizeof(detail->message), "file is larger than this build");
        }
        return DfcTextCapacity;
    }

    P p;
    memset(&p, 0, sizeof(p));
    p.text = text;
    p.len = len;
    p.c = credential;
    p.status = DfcTextOk;

    dfc_credential_clear(credential);

    do {
        if(!prescan(&p)) break;

        Line l;
        if(!required(&p, "Filetype", &l)) break;
        if(l.value_len != 14 || memcmp(l.value, "DFC Credential", 14) != 0) {
            fail(&p, l.number, "Filetype shall be 'DFC Credential'");
            break;
        }
        if(!required(&p, "Version", &l)) break;
        if(l.value_len != 1 || l.value[0] < '0' + DFC_MIN_READ_FORMAT_VERSION ||
           l.value[0] > '0' + DFC_FORMAT_VERSION) {
            fail_class(&p, DfcTextUnsupported, l.number, "Version shall be 4, 5, or 6");
            break;
        }
        unsigned format_version = (unsigned)(l.value[0] - '0');
        p.format_version = format_version;

        size_t index = 0;
        if(!req_token(&p, "Card Generation", GENERATION_NAMES, 3, &index)) break;
        credential->card.generation = (DfcGeneration)(index + 1);
        uint32_t storage = 0;
        if(!req_uint(&p, "Card Storage", 0, U32_MAX, &storage)) break;
        credential->card.storage = (uint32_t)storage;
        bool uid_present = false;
        size_t uid_len = 0;
        if(!opt_hex(
               &p,
               "UID",
               credential->uid,
               sizeof(credential->uid),
               DFC_DESFIRE_UID_LONG_LEN,
               &uid_len,
               &uid_present)) {
            break;
        }
        if(!dfc_uid_length_is_valid(uid_len)) {
            Line next;
            size_t line = peek_line(&p, &next) ? next.number : 0;
            fail(&p, line, "UID: length shall be 4, 7, or 10");
            break;
        }
        credential->uid_len = (uint8_t)uid_len;
        if(!req_token(&p, "UID Provenance", PROVENANCE_NAMES, 3, &index)) break;
        credential->card.uid_provenance = (DfcUidProvenance)index;
        if(!parse_static_signature(&p)) break;
        if(format_version >= 5) {
            if(!opt_hex_exact(
                   &p,
                   "Card Hardware Version",
                   credential->card.hardware_version,
                   sizeof(credential->card.hardware_version),
                   &credential->card.has_hardware_version)) {
                break;
            }
            if(!opt_hex_exact(
                   &p,
                   "Card Software Version",
                   credential->card.software_version,
                   sizeof(credential->card.software_version),
                   &credential->card.has_software_version)) {
                break;
            }
        }

        if(!parse_picc(&p)) break;
        if(!parse_files(&p, "PICC", DFC_FILE_OWNER_PICC)) break;
        if(!parse_applications(&p)) break;

        // Every line shall have been claimed by a rule above (rule 4).
        if(p.consumed != p.total) {
            bool named = false;
            Line bad;
            if(peek_line(&p, &bad)) {
                // Claims only advance the forward cursor, so every significant
                // line before `bad` belongs to the claimed prefix.
                size_t prior_pos = 0;
                size_t prior_number = 0;
                const char* prior_s;
                size_t prior_n;
                while(raw_line(&p, &prior_pos, &prior_s, &prior_n)) {
                    prior_number++;
                    if(prior_number >= bad.number) break;
                    if(line_is_skipped(prior_s, prior_n)) continue;
                    Line prior;
                    if(!split_line(&p, prior_s, prior_n, prior_number, &prior)) break;
                    if(prior.key_len == bad.key_len &&
                       memcmp(prior.key, bad.key, bad.key_len) == 0) {
                        fail(&p, bad.number, "duplicate key %.*s", (int)bad.key_len, bad.key);
                        named = true;
                        break;
                    }
                }
                if(!named) {
                    char name[KEY_MAX];
                    size_t copy = bad.key_len < sizeof(name) ? bad.key_len : sizeof(name) - 1;
                    memcpy(name, bad.key, copy);
                    name[copy] = '\0';
                    fail(&p, bad.number, "unrecognised or out of order key %s", name);
                    named = true;
                }
            }
            if(!named) fail(&p, 0, "unrecognised or out of order keys are present");
            break;
        }
    } while(false);

    if(!p.failed) {
        // Text this accepts is text the binary codec also accepts, so the
        // shared model rules apply here too.
        DfcModelStatus model = dfc_credential_validate_model(credential);
        if(model != DfcDerOk) {
            fail_class(&p, (DfcTextStatus)model, 0, "the model breaks a DFC format rule");
        }
    }

    if(p.failed) {
        if(detail) *detail = p.err;
        return p.status;
    }
    credential->dirty = false;
    return DfcTextOk;
}

DfcTextStatus
    dfc_text_parse(DfcCredential* credential, const char* text, size_t len, DfcTextError* detail) {
    DfcTextStatus status = parse_credential(credential, text, len, detail);
    // One exit for every failure, argument and capacity errors included.
    if(status != DfcTextOk && credential) dfc_credential_clear(credential);
    return status;
}

// ------------------------------------------------------------------ writer ---

// Collects text, or counts it when `buf` is NULL. Sticky overflow, so one check
// at the end covers the whole write.
typedef struct {
    char* buf;
    size_t cap;
    size_t len;
    bool overflow;
} W;

static void w_raw(W* w, const char* s, size_t n) {
    if(n > SIZE_MAX - w->len) {
        w->overflow = true;
        w->len = SIZE_MAX;
        return;
    }
    if(w->buf) {
        if(n > w->cap - DFC_MIN(w->len, w->cap)) {
            w->overflow = true;
        } else if(!w->overflow) {
            memcpy(w->buf + w->len, s, n);
        }
    }
    w->len += n;
}

static void w_str(W* w, const char* s) {
    w_raw(w, s, strlen(s));
}

static void w_line_str(W* w, const char* key, const char* value) {
    w_str(w, key);
    w_raw(w, ": ", 2);
    w_str(w, value);
    w_raw(w, "\n", 1);
}

static void w_line_auth_commands(W* w, const char* key, uint8_t commands) {
    char value[80];
    size_t at = 0;
    if(commands == 0) {
        w_line_str(w, key, "None");
        return;
    }
    for(size_t i = 0; i < sizeof(AUTH_COMMAND_BITS); i++) {
        if(!(commands & AUTH_COMMAND_BITS[i])) continue;
        if(at != 0) {
            value[at++] = ',';
            value[at++] = ' ';
        }
        size_t length = strlen(AUTH_COMMAND_NAMES[i]);
        memcpy(value + at, AUTH_COMMAND_NAMES[i], length);
        at += length;
    }
    value[at] = '\0';
    w_line_str(w, key, value);
}

static void w_line_uint(W* w, const char* key, uint32_t value) {
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)value);
    w_line_str(w, key, tmp);
}

static void w_line_int(W* w, const char* key, int32_t value) {
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%ld", (long)value);
    w_line_str(w, key, tmp);
}

static void w_line_hex(W* w, const char* key, const uint8_t* bytes, size_t n) {
    w_str(w, key);
    w_raw(w, ": ", 2);
    static const char digits[] = "0123456789ABCDEF";
    for(size_t i = 0; i < n; i++) {
        char pair[3] = {digits[bytes[i] >> 4], digits[bytes[i] & 0x0F], ' '};
        w_raw(w, pair, i + 1 < n ? 3 : 2);
    }
    w_raw(w, "\n", 1);
}

static void w_line_bool(W* w, const char* key, bool value) {
    w_line_str(w, key, value ? "1" : "0");
}

// `app` selects an application, NULL the PICC record.
static void write_keys(
    W* w,
    const char* prefix,
    const DfcCredential* c,
    const DfcApplication* app,
    size_t count,
    size_t key_len) {
    char key[KEY_MAX];
    const uint8_t* versions = app ? app->key_versions : c->picc_key_versions;
    for(size_t i = 0; i < count; i++) {
        const uint8_t* value = dfc_credential_key_const(c, app, i);
        if(!value) return;
        snprintf(key, sizeof(key), "%s Key %02X", prefix, (unsigned)i);
        w_line_hex(w, key, value, key_len);
        snprintf(key, sizeof(key), "%s Key %02X Version", prefix, (unsigned)i);
        w_line_hex(w, key, &versions[i], 1);
    }
}

#if DFC_ENABLE_KEY_SETS
static void
    write_key_sets(W* w, const char* prefix, const DfcCredential* c, const DfcApplication* a) {
    char key[KEY_MAX];
    char name[PREFIX_MAX];
    snprintf(key, sizeof(key), "%s Key Set Key Count", prefix);
    w_line_uint(w, key, (uint32_t)a->num_keys);
    snprintf(key, sizeof(key), "%s Key Set Maximum Key Size", prefix);
    w_line_uint(w, key, a->max_key_size);
    snprintf(key, sizeof(key), "%s Key Set Settings", prefix);
    w_line_hex(w, key, &a->key_set_settings, 1);
    snprintf(key, sizeof(key), "%s Key Set Count", prefix);
    w_line_uint(w, key, a->num_key_sets);
    for(size_t set = 0; set < a->num_key_sets; set++) {
        const uint8_t* set_version = dfc_credential_key_set_version_const(c, a, set);
        const uint8_t* set_type = dfc_credential_key_set_type_const(c, a, set);
        if(!set_version || !set_type) {
            w->overflow = true;
            return;
        }
        snprintf(name, sizeof(name), "%s Key Set %02X", prefix, (unsigned)set);
        snprintf(key, sizeof(key), "%s Version", name);
        w_line_hex(w, key, set_version, 1);
        snprintf(key, sizeof(key), "%s Type", name);
        w_line_str(w, key, AUTH_NAMES[*set_type]);
        snprintf(key, sizeof(key), "%s Initialized", name);
        w_line_bool(w, key, dfc_credential_key_set_is_initialized(c, a, set));
        snprintf(key, sizeof(key), "%s Key Count", name);
        w_line_uint(w, key, (uint32_t)a->num_keys);
        for(size_t slot = 0; slot < a->num_keys; slot++) {
            const uint8_t* value = dfc_credential_key_in_set_const(c, a, set, slot);
            if(!value) {
                w->overflow = true;
                return;
            }
            const uint8_t* version_ptr = dfc_credential_key_version_in_set_const(c, a, set, slot);
            if(!version_ptr) {
                w->overflow = true;
                return;
            }
            uint8_t version = *version_ptr;
            snprintf(key, sizeof(key), "%s Key %02X", name, (unsigned)slot);
            size_t key_len = dfc_key_set_type_length(*set_type);
            w_line_hex(w, key, value, key_len);
            snprintf(key, sizeof(key), "%s Key %02X Version", name, (unsigned)slot);
            w_line_hex(w, key, &version, 1);
        }
    }
}
#endif

static DfcTextStatus write_files(W* w, const DfcCredential* c, const char* parent, size_t owner) {
    char key[KEY_MAX];
    char prefix[PREFIX_MAX];
    size_t count = 0;
    for(size_t i = 0; i < c->num_files; i++) {
        if(c->files[i].app_index == owner) count++;
    }
    snprintf(key, sizeof(key), "%s File Count", parent);
    w_line_uint(w, key, (uint32_t)count);

    size_t index = 0;
    for(size_t i = 0; i < c->num_files; i++) {
        const DfcFile* f = &c->files[i];
        if(f->app_index != owner) continue;
        snprintf(prefix, sizeof(prefix), "%s File %02X", parent, (unsigned)index++);

        snprintf(key, sizeof(key), "%s Number", prefix);
        w_line_hex(w, key, &f->number, 1);
        if(f->type > TYPE_TMAC) return DfcTextUnsupported;
        snprintf(key, sizeof(key), "%s Type", prefix);
        w_line_str(w, key, FILE_TYPE_NAMES[f->type]);
        snprintf(key, sizeof(key), "%s Communication Settings", prefix);
        w_line_hex(w, key, &f->comm_settings, 1);
        snprintf(key, sizeof(key), "%s Access Rights", prefix);
        uint8_t rights[2] = {(uint8_t)(f->access_rights >> 8), (uint8_t)(f->access_rights & 0xFF)};
        w_line_hex(w, key, rights, 2);
        if(f->has_iso_file_id) {
            uint8_t fid[2] = {(uint8_t)(f->iso_file_id >> 8), (uint8_t)(f->iso_file_id & 0xFF)};
            snprintf(key, sizeof(key), "%s ISO File ID", prefix);
            w_line_hex(w, key, fid, 2);
        }

        const uint8_t* known = dfc_file_data_const(c, f);
        if(type_is_data(f->type)) {
            uint32_t declared_size;
            if(!dfc_file_get_declared_size(f, &declared_size)) return DfcTextMalformed;
            snprintf(key, sizeof(key), "%s Size", prefix);
            w_line_uint(w, key, declared_size);
            if(known && f->data_len > 0) {
                snprintf(key, sizeof(key), "%s Data", prefix);
                w_line_hex(w, key, known, f->data_len);
            }
            snprintf(key, sizeof(key), "%s Data Complete", prefix);
            w_line_bool(w, key, f->contents_complete);
#if DFC_ENABLE_SDM
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagEnabled)) {
                uint8_t sdm_rights[2] = {
                    (uint8_t)(f->state.data.sdm.access_rights >> 8),
                    (uint8_t)(f->state.data.sdm.access_rights & 0xFF)};
                snprintf(key, sizeof(key), "%s SDM Options", prefix);
                w_line_hex(w, key, &f->state.data.sdm.options, 1);
                snprintf(key, sizeof(key), "%s SDM Access Rights", prefix);
                w_line_hex(w, key, sdm_rights, 2);
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagUidOffset)) {
                    snprintf(key, sizeof(key), "%s SDM UID Offset", prefix);
                    w_line_uint(w, key, f->state.data.sdm.uid_offset);
                }
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagCounterOffset)) {
                    snprintf(key, sizeof(key), "%s SDM Counter Offset", prefix);
                    w_line_uint(w, key, f->state.data.sdm.counter_offset);
                }
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagPiccDataOffset)) {
                    snprintf(key, sizeof(key), "%s SDM PICC Data Offset", prefix);
                    w_line_uint(w, key, f->state.data.sdm.picc_data_offset);
                }
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagMacInputOffset)) {
                    snprintf(key, sizeof(key), "%s SDM MAC Input Offset", prefix);
                    w_line_uint(w, key, f->state.data.sdm.mac_input_offset);
                }
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagMacOffset)) {
                    snprintf(key, sizeof(key), "%s SDM MAC Offset", prefix);
                    w_line_uint(w, key, f->state.data.sdm.mac_offset);
                }
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagEncryptedFile)) {
                    snprintf(key, sizeof(key), "%s SDM Encrypted File Offset", prefix);
                    w_line_uint(w, key, f->state.data.sdm.encrypted_file_offset);
                    snprintf(key, sizeof(key), "%s SDM Encrypted File Length", prefix);
                    w_line_uint(w, key, f->state.data.sdm.encrypted_file_length);
                }
                if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagCounterLimit)) {
                    snprintf(key, sizeof(key), "%s SDM Counter Limit", prefix);
                    w_line_uint(w, key, f->state.data.sdm.counter_limit);
                }
                snprintf(key, sizeof(key), "%s SDM Read Counter", prefix);
                w_line_uint(w, key, f->state.data.sdm_read_counter);
            }
#endif
        } else if(f->type == TYPE_VALUE) {
            snprintf(key, sizeof(key), "%s Value Lower Limit", prefix);
            w_line_int(w, key, f->state.value.lower_limit);
            snprintf(key, sizeof(key), "%s Value Upper Limit", prefix);
            w_line_int(w, key, f->state.value.upper_limit);
            snprintf(key, sizeof(key), "%s Value", prefix);
            w_line_int(w, key, f->state.value.current);
            snprintf(key, sizeof(key), "%s Limited Credit", prefix);
            w_line_hex(w, key, &f->state.value.limited_credit, 1);
            if(f->state.value.limited_credit_value != 0) {
                snprintf(key, sizeof(key), "%s Limited Credit Value", prefix);
                w_line_int(w, key, f->state.value.limited_credit_value);
            }
        } else if(f->type == TYPE_TMAC) {
#if DFC_ENABLE_TRANSACTION_MAC
            snprintf(key, sizeof(key), "%s Transaction Counter", prefix);
            w_line_uint(w, key, f->state.transaction_mac.counter);
            snprintf(key, sizeof(key), "%s Transaction MAC", prefix);
            w_line_hex(w, key, f->state.transaction_mac.mac, sizeof(f->state.transaction_mac.mac));
            snprintf(key, sizeof(key), "%s Transaction Key Type", prefix);
            w_line_uint(w, key, f->state.transaction_mac.key_type);
            snprintf(key, sizeof(key), "%s Transaction Key Version", prefix);
            w_line_hex(w, key, &f->state.transaction_mac.key_version, 1);
            snprintf(key, sizeof(key), "%s Transaction Key", prefix);
            w_line_hex(w, key, f->state.transaction_mac.key, sizeof(f->state.transaction_mac.key));
            snprintf(key, sizeof(key), "%s Previous Reader ID", prefix);
            w_line_hex(
                w,
                key,
                f->state.transaction_mac.previous_reader_id,
                sizeof(f->state.transaction_mac.previous_reader_id));
#else
            // The field is recognized but the build omits the feature.
            return DfcTextUnsupported;
#endif
        } else {
            if(f->state.record.size == 0) return DfcTextMalformed;
            snprintf(key, sizeof(key), "%s Record Size", prefix);
            w_line_uint(w, key, f->state.record.size);
            snprintf(key, sizeof(key), "%s Max Records", prefix);
            w_line_uint(w, key, f->state.record.max_records);
            snprintf(key, sizeof(key), "%s Record Count", prefix);
            w_line_uint(w, key, f->state.record.count);
            size_t stored = f->data_len / f->state.record.size;
            for(size_t r = 0; r < stored && known; r++) {
                snprintf(key, sizeof(key), "%s Record %02X", prefix, (unsigned)r);
                w_line_hex(w, key, known + r * f->state.record.size, f->state.record.size);
            }
            snprintf(key, sizeof(key), "%s Record Complete", prefix);
            w_line_bool(w, key, f->contents_complete);
        }
    }
    return DfcTextOk;
}

static DfcTextStatus write_credential(W* w, const DfcCredential* c) {
    DfcModelStatus model = dfc_credential_validate_model(c);
    if(model != DfcDerOk) return (DfcTextStatus)model;
    if(!dfc_uid_length_is_valid(c->uid_len)) {
        return DfcTextMalformed;
    }
    if(c->card.generation < DfcGenerationEv1 || c->card.generation > DfcGenerationEv3) {
        return DfcTextMalformed;
    }
    if(c->card.uid_provenance > DfcUidProvenanceUnknown) return DfcTextMalformed;
    if(c->picc_num_keys > DFC_MAX_KEYS) return DfcTextMalformed;

    w_line_str(w, "Filetype", "DFC Credential");
    w_line_uint(w, "Version", DFC_FORMAT_VERSION);
    w_line_str(w, "Card Generation", GENERATION_NAMES[c->card.generation - 1]);
    w_line_uint(w, "Card Storage", c->card.storage);
    w_line_hex(w, "UID", c->uid, c->uid_len);
    w_line_str(w, "UID Provenance", PROVENANCE_NAMES[c->card.uid_provenance]);
#if DFC_ENABLE_STATIC_SIGNATURE
    if(c->picc_has_static_signature) {
        w_line_hex(
            w, "Card Static Signature", c->picc_static_signature, sizeof(c->picc_static_signature));
    }
#endif
    if(c->card.has_hardware_version) {
        w_line_hex(
            w, "Card Hardware Version", c->card.hardware_version, sizeof(c->card.hardware_version));
    }
    if(c->card.has_software_version) {
        w_line_hex(
            w, "Card Software Version", c->card.software_version, sizeof(c->card.software_version));
    }

    w_line_hex(w, "PICC Key Settings 1", &c->picc_key_settings_1, 1);
    w_line_hex(w, "PICC Key Settings 2", &c->picc_key_settings_2, 1);
    w_line_auth_commands(w, "PICC Authentication Commands", dfc_credential_picc_auth_commands(c));
    if(c->picc_has_preferred_auth_command) {
        w_line_auth_commands(
            w, "PICC Preferred Authentication Command", c->picc_preferred_auth_command);
    }
    w_line_uint(w, "PICC Key Count", (uint32_t)c->picc_num_keys);
    write_keys(
        w, "PICC", c, NULL, c->picc_num_keys, dfc_credential_stored_key_length(c->picc_key_len));
    // Canonical omission: these carry their default (rule 5).
    if(c->picc_random_id) w_line_bool(w, "PICC Random ID", true);
    if(c->picc_random_id_legacy) w_line_bool(w, "PICC Random ID Legacy Prefix", true);
    if(c->picc_format_disabled) w_line_bool(w, "PICC Format Disabled", true);
    if(c->picc_ats_len > 0) w_line_hex(w, "PICC ATS", c->picc_ats, c->picc_ats_len);
    if(c->picc_has_sak) w_line_hex(w, "PICC SAK", &c->picc_sak, 1);
    if(c->picc_has_atqa) w_line_hex(w, "PICC ATQA", c->picc_atqa, 2);
    if(c->picc_has_sm_disable) w_line_hex(w, "PICC SM Disable", &c->picc_sm_disable, 1);
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(c->picc_has_ev2_capabilities) {
        w_line_hex(
            w,
            "PICC EV2 Card Capabilities",
            c->picc_ev2_capabilities,
            sizeof(c->picc_ev2_capabilities));
    }
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    if(c->picc_has_proximity_key) {
        w_line_hex(w, "PICC Proximity Key", c->picc_proximity_key, sizeof(c->picc_proximity_key));
        w_line_hex(w, "PICC Proximity Option", &c->picc_proximity_option, 1);
        w_line_uint(
            w,
            "PICC Proximity Published Response Time",
            c->picc_proximity_published_response_time);
        if(c->picc_has_proximity_bitrate) {
            w_line_hex(w, "PICC Proximity Bitrate", &c->picc_proximity_bitrate, 1);
        }
    }
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    if(c->virtual_card_configured) {
        w_line_hex(
            w,
            "PICC Virtual Card Installation ID",
            c->virtual_card_installation_id,
            c->virtual_card_installation_id_len);
        w_line_hex(w, "PICC Virtual Card Information", &c->virtual_card_information, 1);
        w_line_hex(
            w,
            "PICC Virtual Card Capabilities",
            c->virtual_card_capabilities,
            sizeof(c->virtual_card_capabilities));
        w_line_hex(w, "PICC Virtual Card UID", c->virtual_card_uid, c->virtual_card_uid_len);
        w_line_hex(
            w,
            "PICC Virtual Card Select MAC Key",
            c->virtual_card_select_mac_key,
            sizeof(c->virtual_card_select_mac_key));
        w_line_hex(
            w,
            "PICC Virtual Card Select Encryption Key",
            c->virtual_card_select_encryption_key,
            sizeof(c->virtual_card_select_encryption_key));
        w_line_bool(
            w,
            "PICC Virtual Card Authentication Mandatory",
            c->virtual_card_authentication_mandatory);
        w_line_bool(
            w, "PICC Virtual Card Proximity Mandatory", c->virtual_card_proximity_mandatory);
    }
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    if(c->picc_has_dam_keys) {
        w_line_hex(
            w, "PICC DAM Authentication Key", c->picc_dam_auth_key, sizeof(c->picc_dam_auth_key));
        w_line_hex(w, "PICC DAM MAC Key", c->picc_dam_mac_key, sizeof(c->picc_dam_mac_key));
        w_line_hex(
            w,
            "PICC DAM Encryption Key",
            c->picc_dam_encryption_key,
            sizeof(c->picc_dam_encryption_key));
    }
#endif

    DfcTextStatus st = write_files(w, c, "PICC", DFC_FILE_OWNER_PICC);
    if(st != DfcTextOk) return st;

    w_line_uint(w, "Application Count", (uint32_t)c->num_apps);
    char prefix[PARENT_MAX];
    char key[KEY_MAX];
    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* a = &c->apps[i];
        if(a->num_keys > DFC_MAX_KEYS) return DfcTextMalformed;
        if(a->iso_aid_len > sizeof(a->iso_aid)) return DfcTextMalformed;
        snprintf(prefix, sizeof(prefix), "Application %02X", (unsigned)(uint8_t)i);

        snprintf(key, sizeof(key), "%s AID", prefix);
        w_line_hex(w, key, a->aid, 3);
        if(a->has_iso_file_id) {
            uint8_t fid[2] = {(uint8_t)(a->iso_file_id >> 8), (uint8_t)(a->iso_file_id & 0xFF)};
            snprintf(key, sizeof(key), "%s ISO File ID", prefix);
            w_line_hex(w, key, fid, 2);
        }
        if(a->iso_aid_len > 0) {
            snprintf(key, sizeof(key), "%s DF Name", prefix);
            w_line_hex(w, key, a->iso_aid, a->iso_aid_len);
        }
        snprintf(key, sizeof(key), "%s Key Settings 1", prefix);
        w_line_hex(w, key, &a->key_settings_1, 1);
        snprintf(key, sizeof(key), "%s Key Settings 2", prefix);
        w_line_hex(w, key, &a->key_settings_2, 1);
        snprintf(key, sizeof(key), "%s Authentication Commands", prefix);
        w_line_auth_commands(w, key, dfc_credential_app_auth_commands(c, a));
        if(a->has_preferred_auth_command) {
            snprintf(key, sizeof(key), "%s Preferred Authentication Command", prefix);
            w_line_auth_commands(w, key, a->preferred_auth_command);
        }
#if DFC_ENABLE_KEY_SETS
        if(a->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT) {
            write_key_sets(w, prefix, c, a);
        } else {
#endif
            snprintf(key, sizeof(key), "%s Key Count", prefix);
            w_line_uint(w, key, (uint32_t)a->num_keys);
            write_keys(w, prefix, c, a, a->num_keys, dfc_credential_stored_key_length(a->key_len));
#if DFC_ENABLE_KEY_SETS
        }
#endif

        st = write_files(w, c, prefix, i);
        if(st != DfcTextOk) return st;

#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
        if(a->has_capability_data) {
            snprintf(key, sizeof(key), "%s Capability Data", prefix);
            w_line_hex(w, key, a->capability_data, sizeof(a->capability_data));
        }
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        if(a->delegated) {
            snprintf(key, sizeof(key), "%s Delegated Slot Number", prefix);
            w_line_uint(w, key, a->delegated_slot_number);
            snprintf(key, sizeof(key), "%s Delegated Slot Version", prefix);
            w_line_hex(w, key, &a->delegated_slot_version, 1);
            snprintf(key, sizeof(key), "%s Delegated Quota Limit", prefix);
            w_line_uint(w, key, a->delegated_quota_limit);
            snprintf(key, sizeof(key), "%s Delegated Free Blocks", prefix);
            w_line_uint(w, key, a->delegated_free_blocks);
            if(a->delegated_format_disabled) {
                snprintf(key, sizeof(key), "%s Delegated Format Disabled", prefix);
                w_line_bool(w, key, true);
            }
        }
#endif
        if(a->has_sm_disable) {
            snprintf(key, sizeof(key), "%s SM Disable", prefix);
            w_line_hex(w, key, &a->sm_disable, 1);
        }
    }
    return DfcTextOk;
}

DfcTextStatus dfc_text_write(const DfcCredential* credential, char* out, size_t cap, size_t* len) {
    if(!credential || !len) return DfcTextMalformed;
    if(!out && cap > 0) return DfcTextMalformed;

    W w = {out, cap, 0, false};
    DfcTextStatus st = write_credential(&w, credential);
    if(st != DfcTextOk) return st;
    *len = w.len;
    if(w.overflow) {
        if(out && cap) out[0] = '\0';
        return DfcTextCapacity;
    }
    if(w.len < cap) out[w.len] = '\0';
    return DfcTextOk;
}

DfcTextStatus dfc_credential_load(
    DfcCredential* credential,
    const uint8_t* content,
    size_t len,
    DfcTextError* detail) {
    if(detail) {
        detail->line = 0;
        detail->message[0] = '\0';
    }
    if(!credential) return DfcTextMalformed;
    if(!content && len > 0) {
        dfc_credential_clear(credential);
        return DfcTextMalformed;
    }
    if(dfc_credential_content_is_binary(content, len)) {
        DfcDerStatus st = dfc_der_decode(credential, content, len);
        if(st != DfcDerOk && detail) {
            snprintf(
                detail->message,
                sizeof(detail->message),
                "binary credential is %s",
                dfc_der_status_name(st));
        }
        return st;
    }
    return dfc_text_parse(credential, (const char*)content, len, detail);
}

#endif // DFC_ENABLE_TEXT_CODEC
