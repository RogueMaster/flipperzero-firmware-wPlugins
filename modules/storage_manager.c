#include "storage_manager.h"
#include <flipper_format/flipper_format.h>

#define RECON_FILE_TYPE    "Flipper Recon Session"
#define RECON_FILE_VERSION 1

void recon_sanitize_filename(const char* name, char* out, size_t out_len) {
    furi_check(out);
    furi_check(out_len > 0);
    size_t w = 0;
    for(const char* p = name ? name : ""; *p && w + 1 < out_len; p++) {
        char c = *p;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == ' ' || c == '.' || c == '-' || c == '_';
        out[w++] = ok ? c : '_';
    }
    out[w] = '\0';
    /* trim trailing spaces/dots which some filesystems dislike */
    while(w > 0 && (out[w - 1] == ' ' || out[w - 1] == '.'))
        out[--w] = '\0';
    if(w == 0) strncpy(out, "engagement", out_len - 1);
}

static void recon_storage_path(FuriString* out, const char* name) {
    char safe[RECON_NAME_LEN];
    recon_sanitize_filename(name, safe, sizeof(safe));
    furi_string_printf(out, "%s/%s%s", RECON_SESSION_DIR, safe, RECON_SESSION_EXT);
}

bool recon_storage_session_exists(Storage* storage, const char* name) {
    furi_check(storage);
    FuriString* path = furi_string_alloc();
    recon_storage_path(path, name);
    bool exists = storage_common_stat(storage, furi_string_get_cstr(path), NULL) == FSE_OK;
    furi_string_free(path);
    return exists;
}

void recon_storage_ensure_dirs(Storage* storage) {
    furi_check(storage);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, RECON_SESSION_DIR);
    storage_common_mkdir(storage, RECON_EXPORT_DIR);
}

static bool write_session(FlipperFormat* ff, const Session* session) {
    bool ok = false;
    uint32_t tmp;
    do {
        if(!flipper_format_write_header_cstr(ff, RECON_FILE_TYPE, RECON_FILE_VERSION)) break;
        if(!flipper_format_write_string_cstr(ff, "SessionName", session->name)) break;
        if(!flipper_format_write_string_cstr(ff, "Client", session->client)) break;
        if(!flipper_format_write_string_cstr(ff, "Location", session->location)) break;
        tmp = session->created;
        if(!flipper_format_write_uint32(ff, "Created", &tmp, 1)) break;
        tmp = session->modified;
        if(!flipper_format_write_uint32(ff, "Modified", &tmp, 1)) break;
        tmp = session->next_id;
        if(!flipper_format_write_uint32(ff, "NextId", &tmp, 1)) break;
        tmp = session->asset_count;
        if(!flipper_format_write_uint32(ff, "AssetCount", &tmp, 1)) break;
        tmp = session->evidence_count;
        if(!flipper_format_write_uint32(ff, "EvidenceCount", &tmp, 1)) break;
        tmp = session->relation_count;
        if(!flipper_format_write_uint32(ff, "RelationCount", &tmp, 1)) break;

        ok = true;
        for(uint16_t i = 0; i < session->asset_count && ok; i++) {
            const Asset* a = &session->assets[i];
            ok = false;
            tmp = a->id;
            if(!flipper_format_write_uint32(ff, "AssetId", &tmp, 1)) break;
            tmp = a->type;
            if(!flipper_format_write_uint32(ff, "AssetType", &tmp, 1)) break;
            tmp = a->risk;
            if(!flipper_format_write_uint32(ff, "AssetRisk", &tmp, 1)) break;
            if(!flipper_format_write_string_cstr(ff, "AssetName", a->name)) break;
            if(!flipper_format_write_string_cstr(ff, "AssetNotes", a->notes)) break;
            tmp = a->created;
            if(!flipper_format_write_uint32(ff, "AssetCreated", &tmp, 1)) break;
            tmp = a->modified;
            if(!flipper_format_write_uint32(ff, "AssetModified", &tmp, 1)) break;
            ok = true;
        }

        for(uint16_t i = 0; i < session->evidence_count && ok; i++) {
            const Evidence* e = &session->evidence[i];
            ok = false;
            tmp = e->id;
            if(!flipper_format_write_uint32(ff, "EvId", &tmp, 1)) break;
            tmp = e->asset_id;
            if(!flipper_format_write_uint32(ff, "EvAsset", &tmp, 1)) break;
            tmp = e->type;
            if(!flipper_format_write_uint32(ff, "EvType", &tmp, 1)) break;
            if(!flipper_format_write_string_cstr(ff, "EvLabel", e->label)) break;
            if(!flipper_format_write_string_cstr(ff, "EvPath", e->path)) break;
            tmp = e->created;
            if(!flipper_format_write_uint32(ff, "EvCreated", &tmp, 1)) break;
            ok = true;
        }

        for(uint16_t i = 0; i < session->relation_count && ok; i++) {
            const Relation* r = &session->relations[i];
            ok = false;
            tmp = r->id;
            if(!flipper_format_write_uint32(ff, "RelId", &tmp, 1)) break;
            tmp = r->from_id;
            if(!flipper_format_write_uint32(ff, "RelFrom", &tmp, 1)) break;
            tmp = r->to_id;
            if(!flipper_format_write_uint32(ff, "RelTo", &tmp, 1)) break;
            tmp = r->type;
            if(!flipper_format_write_uint32(ff, "RelType", &tmp, 1)) break;
            ok = true;
        }
    } while(false);
    return ok;
}

bool recon_storage_save_session(Storage* storage, const Session* session) {
    furi_check(storage);
    furi_check(session);
    recon_storage_ensure_dirs(storage);

    FuriString* path = furi_string_alloc();
    recon_storage_path(path, session->name);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;
    if(flipper_format_file_open_always(ff, furi_string_get_cstr(path))) {
        ok = write_session(ff, session);
    }
    flipper_format_free(ff);
    furi_string_free(path);
    return ok;
}

static void read_str_field(FlipperFormat* ff, const char* key, char* dst, size_t dst_len) {
    FuriString* value = furi_string_alloc();
    if(flipper_format_read_string(ff, key, value)) {
        strncpy(dst, furi_string_get_cstr(value), dst_len - 1);
        dst[dst_len - 1] = '\0';
    }
    furi_string_free(value);
}

bool recon_storage_load_session(Storage* storage, Session* session, const char* name) {
    furi_check(storage);
    furi_check(session);

    FuriString* path = furi_string_alloc();
    recon_storage_path(path, name);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t tmp = 0;
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(ff, file_type, &version)) break;
        if(furi_string_cmp_str(file_type, RECON_FILE_TYPE) != 0) break;

        session_reset(session, name);
        read_str_field(ff, "SessionName", session->name, RECON_NAME_LEN);
        read_str_field(ff, "Client", session->client, RECON_NAME_LEN);
        read_str_field(ff, "Location", session->location, RECON_NAME_LEN);
        if(flipper_format_read_uint32(ff, "Created", &tmp, 1)) session->created = tmp;
        if(flipper_format_read_uint32(ff, "Modified", &tmp, 1)) session->modified = tmp;
        if(flipper_format_read_uint32(ff, "NextId", &tmp, 1)) session->next_id = tmp;

        uint32_t asset_count = 0, evidence_count = 0, relation_count = 0;
        flipper_format_read_uint32(ff, "AssetCount", &asset_count, 1);
        flipper_format_read_uint32(ff, "EvidenceCount", &evidence_count, 1);
        flipper_format_read_uint32(ff, "RelationCount", &relation_count, 1);

        if(asset_count > RECON_MAX_ASSETS) asset_count = RECON_MAX_ASSETS;
        if(evidence_count > RECON_MAX_EVIDENCE) evidence_count = RECON_MAX_EVIDENCE;
        if(relation_count > RECON_MAX_RELATIONS) relation_count = RECON_MAX_RELATIONS;

        for(uint32_t i = 0; i < asset_count; i++) {
            Asset* a = &session->assets[i];
            asset_init(a, 0);
            if(flipper_format_read_uint32(ff, "AssetId", &tmp, 1)) a->id = tmp;
            if(flipper_format_read_uint32(ff, "AssetType", &tmp, 1)) a->type = tmp;
            if(flipper_format_read_uint32(ff, "AssetRisk", &tmp, 1)) a->risk = tmp;
            read_str_field(ff, "AssetName", a->name, RECON_NAME_LEN);
            read_str_field(ff, "AssetNotes", a->notes, RECON_NOTE_LEN);
            if(flipper_format_read_uint32(ff, "AssetCreated", &tmp, 1)) a->created = tmp;
            if(flipper_format_read_uint32(ff, "AssetModified", &tmp, 1)) a->modified = tmp;
        }
        session->asset_count = asset_count;

        for(uint32_t i = 0; i < evidence_count; i++) {
            Evidence* e = &session->evidence[i];
            evidence_init(e, 0, 0);
            if(flipper_format_read_uint32(ff, "EvId", &tmp, 1)) e->id = tmp;
            if(flipper_format_read_uint32(ff, "EvAsset", &tmp, 1)) e->asset_id = tmp;
            if(flipper_format_read_uint32(ff, "EvType", &tmp, 1)) e->type = tmp;
            read_str_field(ff, "EvLabel", e->label, RECON_NAME_LEN);
            read_str_field(ff, "EvPath", e->path, RECON_PATH_LEN);
            if(flipper_format_read_uint32(ff, "EvCreated", &tmp, 1)) e->created = tmp;
        }
        session->evidence_count = evidence_count;

        for(uint32_t i = 0; i < relation_count; i++) {
            Relation* r = &session->relations[i];
            relation_init(r, 0, 0, 0);
            if(flipper_format_read_uint32(ff, "RelId", &tmp, 1)) r->id = tmp;
            if(flipper_format_read_uint32(ff, "RelFrom", &tmp, 1)) r->from_id = tmp;
            if(flipper_format_read_uint32(ff, "RelTo", &tmp, 1)) r->to_id = tmp;
            if(flipper_format_read_uint32(ff, "RelType", &tmp, 1)) r->type = tmp;
        }
        session->relation_count = relation_count;

        session_mark_clean(session);
        ok = true;
    } while(false);

    furi_string_free(file_type);
    flipper_format_free(ff);
    furi_string_free(path);
    return ok;
}

bool recon_storage_delete_session(Storage* storage, const char* name) {
    furi_check(storage);
    FuriString* path = furi_string_alloc();
    recon_storage_path(path, name);
    bool ok = storage_simply_remove(storage, furi_string_get_cstr(path));
    furi_string_free(path);
    return ok;
}

size_t recon_storage_list_sessions(Storage* storage, FuriString** names, size_t capacity) {
    furi_check(storage);
    recon_storage_ensure_dirs(storage);

    File* dir = storage_file_alloc(storage);
    size_t count = 0;
    char name_buf[RECON_NAME_LEN + 8];
    FileInfo info;

    if(storage_dir_open(dir, RECON_SESSION_DIR)) {
        while(count < capacity && storage_dir_read(dir, &info, name_buf, sizeof(name_buf))) {
            if(info.flags & FSF_DIRECTORY) continue;
            char* ext = strstr(name_buf, RECON_SESSION_EXT);
            if(!ext) continue;
            *ext = '\0'; /* strip extension */
            names[count] = furi_string_alloc_set_str(name_buf);
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}
