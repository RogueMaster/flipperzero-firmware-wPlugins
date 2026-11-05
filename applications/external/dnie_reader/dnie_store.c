#include "dnie_store.h"
#include "dnie_country.h"
#include "dnie_mrz.h"

#include <furi_hal_crypto.h>
#include <furi_hal_random.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>

#define TAG        "DnieStore"
#define KEY_SLOT   FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT
#define FILE_MAGIC "DNIEREC1"
#define HEADER_LEN (8 + 16 + 4)

// --- Cifrado con la clave del enclave ---

static bool
    enclave_crypt(bool encrypt, const uint8_t iv[16], const uint8_t* in, uint8_t* out, size_t len) {
    if(!furi_hal_crypto_enclave_ensure_key(KEY_SLOT)) return false;
    if(!furi_hal_crypto_enclave_load_key(KEY_SLOT, iv)) return false;
    bool ok = encrypt ? furi_hal_crypto_encrypt(in, out, len) :
                        furi_hal_crypto_decrypt(in, out, len);
    furi_hal_crypto_enclave_unload_key(KEY_SLOT);
    return ok;
}

// --- Ficheros ---

static DnieStoreResult write_record(const char* path, const DnieRecord* rec) {
    uint8_t* plain = malloc(DNIE_RECORD_MAX);
    uint8_t* cipher = malloc(DNIE_RECORD_MAX);
    DnieStoreResult res = DnieStoreOk;
    size_t n = 0;

    do {
        if(!plain || !cipher) {
            res = DnieStoreErrorCrypto;
            break;
        }
        n = dnie_record_pack(rec, plain, DNIE_RECORD_MAX);
        uint8_t header[HEADER_LEN];
        memcpy(header, FILE_MAGIC, 8);
        furi_hal_random_fill_buf(&header[8], 16); // IV nuevo en cada guardado
        header[24] = (uint8_t)(n >> 24);
        header[25] = (uint8_t)(n >> 16);
        header[26] = (uint8_t)(n >> 8);
        header[27] = (uint8_t)n;
        if(n == 0 || !enclave_crypt(true, &header[8], plain, cipher, n)) {
            res = DnieStoreErrorCrypto;
            break;
        }

        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_simply_mkdir(storage, EXT_PATH("apps_data"));
        storage_simply_mkdir(storage, DNIE_DATA_DIR);
        storage_simply_mkdir(storage, DNIE_SAVED_DIR);
        File* f = storage_file_alloc(storage);
        bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                  storage_file_write(f, header, HEADER_LEN) == HEADER_LEN &&
                  storage_file_write(f, cipher, n) == n;
        storage_file_close(f);
        storage_file_free(f);
        if(!ok) storage_simply_remove(storage, path);
        furi_record_close(RECORD_STORAGE);
        if(!ok) res = DnieStoreErrorFile;
    } while(false);

    if(plain) {
        memset(plain, 0, DNIE_RECORD_MAX); // no dejar datos en claro en el heap
        free(plain);
    }
    free(cipher);
    return res;
}

DnieStoreResult dnie_store_load(const char* path, DnieRecord* out) {
    uint8_t* cipher = malloc(DNIE_RECORD_MAX);
    uint8_t* plain = malloc(DNIE_RECORD_MAX);
    DnieStoreResult res = DnieStoreOk;

    do {
        if(!plain || !cipher) {
            res = DnieStoreErrorCrypto;
            break;
        }
        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* f = storage_file_alloc(storage);
        uint8_t header[HEADER_LEN];
        size_t n = 0;
        bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                  storage_file_read(f, header, HEADER_LEN) == HEADER_LEN &&
                  memcmp(header, FILE_MAGIC, 8) == 0;
        if(ok) {
            n = (size_t)header[24] << 24 | (size_t)header[25] << 16 | (size_t)header[26] << 8 |
                header[27];
            ok = n > 0 && n <= DNIE_RECORD_MAX && n % 16 == 0 &&
                 storage_file_read(f, cipher, n) == n;
        }
        storage_file_close(f);
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        if(!ok) {
            res = DnieStoreErrorFile;
            break;
        }
        if(!enclave_crypt(false, &header[8], cipher, plain, n)) {
            res = DnieStoreErrorCrypto;
            break;
        }
        // El SHA-256 interno falla si el fichero esta corrupto o es de otro Flipper
        if(!dnie_record_unpack(plain, n, out)) res = DnieStoreErrorFormat;
    } while(false);

    if(plain) {
        memset(plain, 0, DNIE_RECORD_MAX);
        free(plain);
    }
    free(cipher);
    return res;
}

// Recorre los ficheros .dnie del directorio. cb devuelve false para parar.
typedef bool (*EntryCallback)(const char* path, void* context);

static void for_each_entry(EntryCallback cb, void* context) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    char name[64];
    char path[128];
    if(storage_dir_open(dir, DNIE_SAVED_DIR)) {
        FileInfo info;
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            size_t len = strlen(name);
            if(len < 5 || strcmp(&name[len - 5], ".dnie") != 0) continue;
            snprintf(path, sizeof(path), "%s/%s", DNIE_SAVED_DIR, name);
            if(!cb(path, context)) break;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

// --- Guardar (sustituyendo una lectura previa del mismo DNI) ---

typedef struct {
    char key[80];
    char found[128];
    size_t count;
    DnieRecord* tmp;
} FindCtx;

// Identifica a la persona: DNI en las completas, nombre + apellidos + cumple en las de album
static bool record_key(const DnieRecord* r, char* out, size_t size) {
    if(r->kind == DnieRecordFull) {
        DnieMrz mrz;
        if(!dnie_mrz_parse_dg1(r->dg1, r->dg1_len, &mrz)) return false;
        snprintf(out, size, "F%s", mrz.personal_number);
        memset(&mrz, 0, sizeof(mrz));
    } else {
        snprintf(out, size, "A%s|%s|%.8s", r->given_names, r->surnames, r->birth);
    }
    return true;
}

static bool find_same_person(const char* path, void* context) {
    FindCtx* f = context;
    f->count++;
    char key[80];
    if(dnie_store_load(path, f->tmp) == DnieStoreOk && record_key(f->tmp, key, sizeof(key)) &&
       strcmp(key, f->key) == 0) {
        strlcpy(f->found, path, sizeof(f->found));
    }
    memset(f->tmp, 0, sizeof(DnieRecord));
    return true;
}

DnieStoreResult dnie_store_save(const DnieRecord* rec) {
    FindCtx f = {.found = {0}, .count = 0};
    if(!record_key(rec, f.key, sizeof(f.key))) return DnieStoreErrorFormat;
    f.tmp = malloc(sizeof(DnieRecord));
    if(!f.tmp) return DnieStoreErrorCrypto;
    // Ojo: for_each_entry abre el storage; dnie_store_load tambien (el record es reentrante)
    for_each_entry(find_same_person, &f);
    free(f.tmp);

    char path[128];
    if(f.found[0]) {
        strlcpy(path, f.found, sizeof(path));
        // Se sustituye la lectura de la misma persona: conservar el nombre que le pusiste
        if(!rec->alias[0]) {
            DnieRecord* old = malloc(sizeof(DnieRecord));
            if(old && dnie_store_load(path, old) == DnieStoreOk && old->alias[0]) {
                DnieRecord* copy = malloc(sizeof(DnieRecord));
                if(copy) {
                    *copy = *rec;
                    strlcpy(copy->alias, old->alias, sizeof(copy->alias));
                    DnieStoreResult r = write_record(path, copy);
                    memset(copy, 0, sizeof(DnieRecord));
                    free(copy);
                    memset(old, 0, sizeof(DnieRecord));
                    free(old);
                    return r;
                }
            }
            if(old) {
                memset(old, 0, sizeof(DnieRecord));
                free(old);
            }
        }
    } else {
        if(f.count >= DNIE_STORE_MAX) return DnieStoreErrorFull;
        // Nombre neutro (sin datos personales): la fecha en segundos
        snprintf(path, sizeof(path), "%s/lectura_%lu.dnie", DNIE_SAVED_DIR, rec->saved_at);
    }
    return write_record(path, rec);
}

// --- Listar ---

typedef struct {
    DnieSavedEntry* out;
    size_t max;
    size_t n;
    DnieRecord* tmp;
} ListCtx;

static bool list_one(const char* path, void* context) {
    ListCtx* l = context;
    if(l->n >= l->max) return false;
    DnieSavedEntry* e = &l->out[l->n];
    strlcpy(e->path, path, sizeof(e->path));
    DnieMrz mrz;
    bool ok = dnie_store_load(path, l->tmp) == DnieStoreOk;
    const char* given_src = NULL;
    const char* issuer = "";
    const char* sur_src = NULL;
    if(ok && l->tmp->kind == DnieRecordFull &&
       dnie_mrz_parse_dg1(l->tmp->dg1, l->tmp->dg1_len, &mrz)) {
        given_src = mrz.given_names;
        sur_src = mrz.surnames;
        issuer = mrz.issuer;
    } else if(ok && l->tmp->kind == DnieRecordAlbum) {
        given_src = l->tmp->given_names;
        sur_src = l->tmp->surnames;
        issuer = l->tmp->issuer;
    }
    if(given_src) {
        char given[31];
        strlcpy(given, given_src, sizeof(given));
        char* sp = strchr(given, ' ');
        if(sp) *sp = '\0';
        char surname[31];
        strlcpy(surname, sur_src, sizeof(surname));
        sp = strchr(surname, ' ');
        if(sp) *sp = '\0';
        e->kind = l->tmp->kind;
        if(l->tmp->alias[0]) {
            // Nombre puesto por el usuario
            strlcpy(given, l->tmp->alias, sizeof(given));
            surname[0] = '\0';
        }
        snprintf(
            e->label,
            sizeof(e->label),
            "%s%s%s%s%s%s",
            given,
            surname[0] ? " " : "",
            surname,
            issuer[0] ? " - " : "",
            dnie_country_name(issuer),
            l->tmp->kind == DnieRecordFull ? " (ID)" : "");
        memset(&mrz, 0, sizeof(mrz));
    } else {
        strlcpy(e->label, "(unreadable)", sizeof(e->label)); // corrupto o de otro Flipper
    }
    memset(l->tmp, 0, sizeof(DnieRecord));
    l->n++;
    return true;
}

size_t dnie_store_list(DnieSavedEntry* out, size_t max) {
    ListCtx l = {.out = out, .max = max, .n = 0};
    l.tmp = malloc(sizeof(DnieRecord));
    if(!l.tmp) return 0;
    for_each_entry(list_one, &l);
    free(l.tmp);
    return l.n;
}

bool dnie_store_delete(const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

DnieStoreResult dnie_store_rename(const char* path, const char* alias) {
    DnieRecord* rec = malloc(sizeof(DnieRecord));
    if(!rec) return DnieStoreErrorCrypto;
    DnieStoreResult r = dnie_store_load(path, rec);
    if(r == DnieStoreOk) {
        strlcpy(rec->alias, alias, sizeof(rec->alias));
        r = write_record(path, rec);
    }
    memset(rec, 0, sizeof(DnieRecord));
    free(rec);
    return r;
}
