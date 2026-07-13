#include "capture_meta.h"
#include <flipper_format/flipper_format.h>

static bool has_ext(const char* path, const char* ext) {
    size_t lp = strlen(path);
    size_t le = strlen(ext);
    if(le > lp) return false;
    return strcasecmp(path + lp - le, ext) == 0;
}

EvidenceType capture_meta_type_from_path(const char* path) {
    if(!path) return EvidenceNote;
    if(has_ext(path, ".sub")) return EvidenceRf;
    if(has_ext(path, ".nfc")) return EvidenceNfc;
    return EvidenceNote;
}

static bool extract_sub(FlipperFormat* ff, char* out, size_t out_len) {
    uint32_t freq = 0;
    FuriString* preset = furi_string_alloc();
    FuriString* proto = furi_string_alloc();
    bool got = false;

    flipper_format_read_uint32(ff, "Frequency", &freq, 1);
    flipper_format_rewind(ff);
    flipper_format_read_string(ff, "Preset", preset);
    flipper_format_rewind(ff);
    flipper_format_read_string(ff, "Protocol", proto);

    if(freq > 0) {
        uint32_t mhz = freq / 1000000;
        uint32_t frac = (freq % 1000000) / 10000;
        const char* extra = furi_string_size(proto)  ? furi_string_get_cstr(proto) :
                            furi_string_size(preset) ? furi_string_get_cstr(preset) :
                                                       "";
        snprintf(out, out_len, "%lu.%02lu MHz %s", (unsigned long)mhz, (unsigned long)frac, extra);
        got = true;
    }
    furi_string_free(preset);
    furi_string_free(proto);
    return got;
}

static bool extract_nfc(FlipperFormat* ff, char* out, size_t out_len) {
    FuriString* dev = furi_string_alloc();
    bool got = false;

    flipper_format_read_string(ff, "Device type", dev);
    flipper_format_rewind(ff);

    uint32_t uid_len = 0;
    char uid_hex[24] = {0};
    if(flipper_format_get_value_count(ff, "UID", &uid_len) && uid_len > 0 && uid_len <= 10) {
        uint8_t uid[10] = {0};
        if(flipper_format_read_hex(ff, "UID", uid, uid_len)) {
            size_t w = 0;
            for(uint32_t i = 0; i < uid_len && w + 2 < sizeof(uid_hex); i++) {
                w += snprintf(uid_hex + w, sizeof(uid_hex) - w, "%02X", uid[i]);
            }
        }
    }

    if(furi_string_size(dev) || uid_hex[0]) {
        snprintf(
            out,
            out_len,
            "%s %s",
            furi_string_size(dev) ? furi_string_get_cstr(dev) : "NFC",
            uid_hex);
        got = true;
    }
    furi_string_free(dev);
    return got;
}

bool capture_meta_extract(Storage* storage, const char* path, char* out_info, size_t out_len) {
    furi_check(storage);
    if(!path || !out_info || out_len == 0) return false;
    out_info[0] = '\0';

    EvidenceType type = capture_meta_type_from_path(path);
    if(type != EvidenceRf && type != EvidenceNfc) return false;

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* ftype = furi_string_alloc();
    uint32_t version = 0;
    bool ok = false;

    if(flipper_format_file_open_existing(ff, path) &&
       flipper_format_read_header(ff, ftype, &version)) {
        if(type == EvidenceRf) {
            ok = extract_sub(ff, out_info, out_len);
        } else {
            ok = extract_nfc(ff, out_info, out_len);
        }
    }

    furi_string_free(ftype);
    flipper_format_free(ff);
    return ok;
}
