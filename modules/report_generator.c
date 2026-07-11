#include "report_generator.h"
#include "asset_manager.h"
#include "graph_engine.h"

/* Append text to a file, returning false on short write. */
static bool file_puts(File* file, const char* text) {
    size_t len = strlen(text);
    return storage_file_write(file, text, len) == len;
}

/* Append a JSON string value with the required escaping. */
static bool file_put_json_string(File* file, const char* text) {
    if(!file_puts(file, "\"")) return false;
    char esc[2];
    esc[1] = '\0';
    for(const char* p = text; *p; p++) {
        char c = *p;
        if(c == '"' || c == '\\') {
            if(!file_puts(file, "\\")) return false;
            esc[0] = c;
            if(!file_puts(file, esc)) return false;
        } else if(c == '\n') {
            if(!file_puts(file, "\\n")) return false;
        } else if(c == '\t') {
            if(!file_puts(file, "\\t")) return false;
        } else if((unsigned char)c < 0x20) {
            continue; /* drop other control chars */
        } else {
            esc[0] = c;
            if(!file_puts(file, esc)) return false;
        }
    }
    return file_puts(file, "\"");
}

static bool file_put_kv_str(File* file, const char* key, const char* value, bool comma) {
    FuriString* line = furi_string_alloc();
    furi_string_printf(line, "    \"%s\": ", key);
    bool ok = file_puts(file, furi_string_get_cstr(line));
    furi_string_free(line);
    if(ok) ok = file_put_json_string(file, value);
    if(ok) ok = file_puts(file, comma ? ",\n" : "\n");
    return ok;
}

static bool file_put_kv_num(File* file, const char* key, uint32_t value, bool comma) {
    FuriString* line = furi_string_alloc();
    furi_string_printf(line, "    \"%s\": %lu%s\n", key, (unsigned long)value, comma ? "," : "");
    bool ok = file_puts(file, furi_string_get_cstr(line));
    furi_string_free(line);
    return ok;
}

static void export_path(FuriString* out, const char* name, const char* ext) {
    furi_string_printf(out, "%s/%s%s", RECON_EXPORT_DIR, name, ext);
}

bool report_export_json(Storage* storage, const Session* session, FuriString* out_path) {
    furi_check(storage);
    furi_check(session);
    storage_common_mkdir(storage, RECON_EXPORT_DIR);

    FuriString* path = furi_string_alloc();
    export_path(path, session->name, ".json");

    File* file = storage_file_alloc(storage);
    FuriString* buf = furi_string_alloc();
    bool ok = false;

    do {
        if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS))
            break;

        if(!file_puts(file, "{\n")) break;
        if(!file_put_kv_str(file, "tool", "Flipper Recon", true)) break;
        if(!file_put_kv_num(file, "schema", 1, true)) break;
        if(!file_put_kv_str(file, "name", session->name, true)) break;
        if(!file_put_kv_str(file, "client", session->client, true)) break;
        if(!file_put_kv_str(file, "location", session->location, true)) break;
        if(!file_put_kv_num(file, "created", session->created, true)) break;
        if(!file_put_kv_num(file, "modified", session->modified, true)) break;

        /* assets */
        if(!file_puts(file, "    \"assets\": [\n")) break;
        bool inner_ok = true;
        for(uint16_t i = 0; i < session->asset_count && inner_ok; i++) {
            const Asset* a = &session->assets[i];
            furi_string_printf(
                buf,
                "        {\"id\": %u, \"type\": \"%s\", \"risk\": %u, \"name\": ",
                a->id,
                asset_type_name(a->type),
                a->risk);
            inner_ok = file_puts(file, furi_string_get_cstr(buf));
            if(inner_ok) inner_ok = file_put_json_string(file, a->name);
            if(inner_ok) inner_ok = file_puts(file, ", \"notes\": ");
            if(inner_ok) inner_ok = file_put_json_string(file, a->notes);
            if(inner_ok) inner_ok = file_puts(file, ", \"evidence\": [");
            /* linked evidence ids */
            bool first = true;
            for(uint16_t j = 0; j < session->evidence_count && inner_ok; j++) {
                if(session->evidence[j].asset_id != a->id) continue;
                furi_string_printf(buf, "%s%u", first ? "" : ", ", session->evidence[j].id);
                inner_ok = file_puts(file, furi_string_get_cstr(buf));
                first = false;
            }
            if(inner_ok) {
                inner_ok = file_puts(file, (i + 1 < session->asset_count) ? "]},\n" : "]}\n");
            }
        }
        if(!inner_ok) break;
        if(!file_puts(file, "    ],\n")) break;

        /* evidence */
        if(!file_puts(file, "    \"evidence\": [\n")) break;
        inner_ok = true;
        for(uint16_t i = 0; i < session->evidence_count && inner_ok; i++) {
            const Evidence* e = &session->evidence[i];
            furi_string_printf(
                buf,
                "        {\"id\": %u, \"asset\": %u, \"type\": \"%s\", \"label\": ",
                e->id,
                e->asset_id,
                evidence_type_name(e->type));
            inner_ok = file_puts(file, furi_string_get_cstr(buf));
            if(inner_ok) inner_ok = file_put_json_string(file, e->label);
            if(inner_ok) inner_ok = file_puts(file, ", \"path\": ");
            if(inner_ok) inner_ok = file_put_json_string(file, e->path);
            if(inner_ok)
                inner_ok = file_puts(file, (i + 1 < session->evidence_count) ? "},\n" : "}\n");
        }
        if(!inner_ok) break;
        if(!file_puts(file, "    ],\n")) break;

        /* relations */
        if(!file_puts(file, "    \"relations\": [\n")) break;
        inner_ok = true;
        for(uint16_t i = 0; i < session->relation_count && inner_ok; i++) {
            const Relation* r = &session->relations[i];
            furi_string_printf(
                buf,
                "        {\"from\": %u, \"to\": %u, \"type\": \"%s\"}%s\n",
                r->from_id,
                r->to_id,
                relation_type_name(r->type),
                (i + 1 < session->relation_count) ? "," : "");
            inner_ok = file_puts(file, furi_string_get_cstr(buf));
        }
        if(!inner_ok) break;
        if(!file_puts(file, "    ]\n}\n")) break;

        ok = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(buf);

    if(ok && out_path) furi_string_set(out_path, path);
    furi_string_free(path);
    return ok;
}

static bool md_line(File* file, FuriString* buf) {
    furi_string_cat_str(buf, "\n");
    bool ok = file_puts(file, furi_string_get_cstr(buf));
    furi_string_reset(buf);
    return ok;
}

bool report_export_markdown(Storage* storage, const Session* session, FuriString* out_path) {
    furi_check(storage);
    furi_check(session);
    storage_common_mkdir(storage, RECON_EXPORT_DIR);

    FuriString* path = furi_string_alloc();
    export_path(path, session->name, ".md");

    File* file = storage_file_alloc(storage);
    FuriString* line = furi_string_alloc();
    bool ok = false;

    do {
        if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS))
            break;

        furi_string_printf(line, "# %s", session->name);
        if(!md_line(file, line)) break;
        furi_string_printf(
            line,
            "Client: %s  |  Location: %s",
            session->client[0] ? session->client : "-",
            session->location[0] ? session->location : "-");
        if(!md_line(file, line)) break;
        furi_string_printf(
            line,
            "Assets: %u  |  Evidence: %u  |  Relations: %u",
            session->asset_count,
            session->evidence_count,
            session->relation_count);
        if(!md_line(file, line)) break;

        /* Assets with propagated risk. */
        furi_string_set(line, "\n## Assets");
        if(!md_line(file, line)) break;

        uint8_t* prop =
            malloc(sizeof(uint8_t) * (session->asset_count ? session->asset_count : 1));
        graph_propagate_risk(session, prop);

        bool inner_ok = true;
        for(uint16_t i = 0; i < session->asset_count && inner_ok; i++) {
            const Asset* a = &session->assets[i];
            furi_string_printf(
                line,
                "\n### %s (%s)\n- Risk: %u (effective %u)",
                a->name,
                asset_type_name(a->type),
                a->risk,
                prop[i]);
            inner_ok = md_line(file, line);
            if(inner_ok && a->notes[0]) {
                furi_string_printf(line, "- Notes: %s", a->notes);
                inner_ok = md_line(file, line);
            }
            /* evidence for this asset */
            for(uint16_t j = 0; j < session->evidence_count && inner_ok; j++) {
                const Evidence* e = &session->evidence[j];
                if(e->asset_id != a->id) continue;
                if(e->path[0]) {
                    furi_string_printf(
                        line,
                        "- Evidence [%s]: %s (%s)",
                        evidence_type_name(e->type),
                        e->label,
                        e->path);
                } else {
                    furi_string_printf(
                        line, "- Evidence [%s]: %s", evidence_type_name(e->type), e->label);
                }
                inner_ok = md_line(file, line);
            }
        }
        free(prop);
        if(!inner_ok) break;

        /* Relationship graph. */
        furi_string_set(line, "\n## Relationships");
        if(!md_line(file, line)) break;
        inner_ok = true;
        for(uint16_t i = 0; i < session->relation_count && inner_ok; i++) {
            const Relation* r = &session->relations[i];
            uint16_t fi = asset_manager_index_by_id(session, r->from_id);
            uint16_t ti = asset_manager_index_by_id(session, r->to_id);
            const char* from = (fi != RECON_INVALID_INDEX) ? session->assets[fi].name : "?";
            const char* to = (ti != RECON_INVALID_INDEX) ? session->assets[ti].name : "?";
            furi_string_printf(line, "- %s --%s--> %s", from, relation_type_name(r->type), to);
            inner_ok = md_line(file, line);
        }
        if(!inner_ok) break;

        ok = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(line);

    if(ok && out_path) furi_string_set(out_path, path);
    furi_string_free(path);
    return ok;
}
