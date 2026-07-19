#include "ctf_store.h"
#include "ctf_starter.h" /* generated: const char CTF_STARTER_PACK[] */
#include <string.h>
#include <stdio.h>

#define CTF_MAX_FILE_BYTES 16384

/* ------------------------------------------------------------- file read */
/* Read a whole file into a freshly malloc'd, NUL-terminated buffer.
 * Caller frees. Returns NULL on failure. */
static char* read_all(Storage* storage, const char* path) {
    File* f = storage_file_alloc(storage);
    char* buf = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > CTF_MAX_FILE_BYTES) sz = CTF_MAX_FILE_BYTES;
        buf = malloc((size_t)sz + 1);
        if(buf) {
            uint16_t got = storage_file_read(f, buf, (uint16_t)sz);
            buf[got] = '\0';
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return buf;
}

static bool ends_with_pack(const char* name) {
    size_t l = strlen(name);
    if(l < 6) return false;
    const char* e = name + l - 5;
    return (e[0] == '.') && (e[1] == 'p' || e[1] == 'P') && (e[2] == 'a' || e[2] == 'A') &&
           (e[3] == 'c' || e[3] == 'C') && (e[4] == 'k' || e[4] == 'K');
}

/* -------------------------------------------------------------- starter */
void ctf_store_ensure_starter(Storage* storage) {
    storage_common_mkdir(storage, GAUNTLET_BASE_DIR);
    storage_common_mkdir(storage, GAUNTLET_PACKS_DIR);

    /* any .pack already present? then leave the card alone */
    bool has_any = false;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, GAUNTLET_PACKS_DIR)) {
        FileInfo info;
        char name[CTF_FILENAME_LEN];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(!(info.flags & FSF_DIRECTORY) && ends_with_pack(name)) {
                has_any = true;
                break;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    if(has_any) return;

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", GAUNTLET_PACKS_DIR, "starter.pack");
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, CTF_STARTER_PACK, strlen(CTF_STARTER_PACK));
    }
    storage_file_close(f);
    storage_file_free(f);
}

/* ----------------------------------------------------------------- scan */
uint8_t ctf_store_scan(Storage* storage, CtfPackInfo* infos, uint8_t max) {
    uint8_t n = 0;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, GAUNTLET_PACKS_DIR)) {
        FileInfo info;
        char name[CTF_FILENAME_LEN];
        while(n < max && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(!ends_with_pack(name)) continue;

            char path[192];
            snprintf(path, sizeof(path), "%s/%s", GAUNTLET_PACKS_DIR, name);
            char* buf = read_all(storage, path);
            if(!buf) continue;

            CtfPackInfo* pi = &infos[n];
            memset(pi, 0, sizeof(*pi));
            strncpy(pi->filename, name, sizeof(pi->filename) - 1);
            if(ctf_pack_scan(buf, pi)) n++;
            free(buf);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return n;
}

bool ctf_store_load_pack(Storage* storage, const char* filename, CtfPack* out) {
    char path[192];
    snprintf(path, sizeof(path), "%s/%s", GAUNTLET_PACKS_DIR, filename);
    char* buf = read_all(storage, path);
    if(!buf) return false;
    bool ok = ctf_pack_parse(buf, out);
    free(buf);
    return ok;
}

/* ------------------------------------------------------------- progress */
void ctf_store_progress_load(Storage* storage, CtfProgress* p) {
    memset(p, 0, sizeof(*p));
    char* buf = read_all(storage, GAUNTLET_PROGRESS);
    if(!buf) return;

    char* line = buf;
    while(*line) {
        char* nl = strchr(line, '\n');
        if(nl) *nl = '\0';
        if(line[0] == 'S' && line[1] == 'C') { /* "SCORE n" */
            const char* v = line + 2;
            while(*v && (*v < '0' || *v > '9')) v++;
            p->score = (uint32_t)strtoul(v, NULL, 10);
        } else if(line[0] == 'S' && line[1] == ' ') { /* "S <hex>" solved id */
            if(p->solved_count < CTF_MAX_SOLVED) {
                p->solved[p->solved_count++] = (uint32_t)strtoul(line + 2, NULL, 16);
            }
        }
        if(!nl) break;
        line = nl + 1;
    }
    free(buf);
}

void ctf_store_progress_save(Storage* storage, const CtfProgress* p) {
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, GAUNTLET_PROGRESS, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[32];
        int len = snprintf(line, sizeof(line), "GAUNTLET 1\nSCORE %lu\n", (unsigned long)p->score);
        storage_file_write(f, line, len);
        for(uint16_t i = 0; i < p->solved_count; i++) {
            len = snprintf(line, sizeof(line), "S %08lX\n", (unsigned long)p->solved[i]);
            storage_file_write(f, line, len);
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

bool ctf_progress_is_solved(const CtfProgress* p, uint32_t id) {
    for(uint16_t i = 0; i < p->solved_count; i++)
        if(p->solved[i] == id) return true;
    return false;
}

bool ctf_progress_mark_solved(CtfProgress* p, uint32_t id, uint16_t points) {
    if(ctf_progress_is_solved(p, id)) return false;
    if(p->solved_count < CTF_MAX_SOLVED) p->solved[p->solved_count++] = id;
    p->score += points;
    return true;
}

void ctf_progress_reset(CtfProgress* p) {
    memset(p, 0, sizeof(*p));
}

uint8_t ctf_progress_pack_solved(const CtfProgress* p, const CtfPack* pack) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < pack->count; i++)
        if(ctf_progress_is_solved(p, pack->challenges[i].id)) n++;
    return n;
}

/* ----------------------------------------------------------------- rank */
const char* ctf_rank(uint32_t score) {
    if(score >= 3000) return "Legend";
    if(score >= 2000) return "Ghost";
    if(score >= 1200) return "Cipherpunk";
    if(score >= 700) return "Breaker";
    if(score >= 350) return "Operator";
    if(score >= 100) return "Initiate";
    return "Rookie";
}
