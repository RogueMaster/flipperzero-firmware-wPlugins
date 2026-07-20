#include "glitch_storage.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define GLITCH_DIR "/ext/apps_data/glitch_trigger"
#define GLITCH_PROFILE_DIR GLITCH_DIR "/profiles"
#define GLITCH_PROFILE_EXT ".gt"
#define GLITCH_HITS_PATH GLITCH_DIR "/hits.csv"

#define GLITCH_PROFILE_MAGIC 0x47543031UL // "GT01"
#define GLITCH_PROFILE_VER 2

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    GlitchParams params;
} GlitchProfileBlob;

static void glitch_profile_path(const char* name, char* out, size_t out_len) {
    snprintf(out, out_len, "%s/%s%s", GLITCH_PROFILE_DIR, name, GLITCH_PROFILE_EXT);
}

bool glitch_storage_save_profile(const char* name, const GlitchParams* p) {
    furi_assert(name);
    furi_assert(p);
    if(name[0] == '\0') return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, GLITCH_DIR);
    storage_common_mkdir(storage, GLITCH_PROFILE_DIR);

    char path[128];
    glitch_profile_path(name, path, sizeof(path));

    GlitchProfileBlob blob = {
        .magic = GLITCH_PROFILE_MAGIC,
        .version = GLITCH_PROFILE_VER,
        .size = sizeof(GlitchParams),
        .params = *p,
    };

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, &blob, sizeof(blob)) == sizeof(blob);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool glitch_storage_load_profile(const char* name, GlitchParams* p) {
    furi_assert(name);
    furi_assert(p);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    char path[128];
    glitch_profile_path(name, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    GlitchProfileBlob blob;
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t read = storage_file_read(file, &blob, sizeof(blob));
        if(read == sizeof(blob) && blob.magic == GLITCH_PROFILE_MAGIC &&
           blob.version == GLITCH_PROFILE_VER && blob.size == sizeof(GlitchParams)) {
            *p = blob.params;
            ok = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool glitch_storage_delete_profile(const char* name) {
    furi_assert(name);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    char path[128];
    glitch_profile_path(name, path, sizeof(path));
    FS_Error err = storage_common_remove(storage, path);
    furi_record_close(RECORD_STORAGE);
    return err == FSE_OK || err == FSE_NOT_EXIST;
}

size_t glitch_storage_list_profiles(char names[][GLITCH_PROFILE_NAME_MAX], size_t max) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    size_t count = 0;

    if(storage_dir_open(dir, GLITCH_PROFILE_DIR)) {
        FileInfo info;
        char fname[GLITCH_PROFILE_NAME_MAX + 8];
        while(count < max && storage_dir_read(dir, &info, fname, sizeof(fname))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(fname);
            size_t ext = strlen(GLITCH_PROFILE_EXT);
            if(len <= ext || strcmp(fname + len - ext, GLITCH_PROFILE_EXT) != 0) continue;
            len -= ext; // strip ".gt"
            if(len >= GLITCH_PROFILE_NAME_MAX) len = GLITCH_PROFILE_NAME_MAX - 1;
            memcpy(names[count], fname, len);
            names[count][len] = '\0';
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return count;
}

void glitch_storage_log_hit(const GlitchParams* p, uint32_t shot_index, const char* source) {
    furi_assert(p);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, GLITCH_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, GLITCH_HITS_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if(storage_file_tell(file) == 0) {
            const char* hdr = "time_ms,delay_us,width_ns,pulses,gap_us,shot,source\n";
            storage_file_write(file, hdr, strlen(hdr));
        }
        char line[96];
        int n = snprintf(
            line,
            sizeof(line),
            "%lu,%lu,%lu,%u,%lu,%lu,%s\n",
            (unsigned long)furi_get_tick(),
            (unsigned long)p->delay_us,
            (unsigned long)p->width_ns,
            (unsigned)p->pulses,
            (unsigned long)p->gap_us,
            (unsigned long)shot_index,
            source ? source : "?");
        if(n > 0) storage_file_write(file, line, (size_t)n);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
