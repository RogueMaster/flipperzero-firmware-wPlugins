/*
 * SD-card persistence. Everything lives under /ext/apps_data/ssh_runner/ (the SDK's
 * APP_DATA_PATH for the appid); parsing and formatting are the host-tested routines in core/.
 * Passwords are never written here (docs/SECURITY.md).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "ssh_runner_i.h"
#include "core/ffkv.h"
#include "core/cstr.h"

#include <furi_hal_rtc.h>

void file_buf_acquire(SshRunnerApp* app) {
    if(!app->file_buf) app->file_buf = malloc(FILE_BUF_SIZE);
    app->file_buf_depth++;
}

void file_buf_release(SshRunnerApp* app) {
    furi_check(app->file_buf_depth);
    if(--app->file_buf_depth) return;
    memset(app->file_buf, 0, FILE_BUF_SIZE);
    free(app->file_buf);
    app->file_buf = NULL;
}

static bool read_text(SshRunnerApp* app, const char* path) {
    bool ok = false;
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t n = storage_file_read(f, app->file_buf, FILE_BUF_SIZE - 1);
        app->file_buf[n] = '\0';
        ok = true;
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

static bool write_text(SshRunnerApp* app, const char* path, const char* text) {
    bool ok = false;
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t n = strlen(text);
        ok = storage_file_write(f, text, n) == n;
    }
    storage_file_close(f);
    storage_file_free(f);
    if(!ok) FURI_LOG_E(TAG, "write failed: %s", path);
    return ok;
}

void store_init(SshRunnerApp* app) {
    storage_simply_mkdir(app->storage, DATA_DIR);
    storage_simply_mkdir(app->storage, HOSTS_DIR);
    storage_simply_mkdir(app->storage, SCRIPTS_DIR);
    storage_simply_mkdir(app->storage, KEYS_DIR);
}

static bool store_load_settings_body(SshRunnerApp* app) {
    app->settings.mock = false;
    app->settings.default_key[0] = '\0';
    app->settings.remember_notice_seen = false;
    app->settings.examples_seeded = false;
    if(!read_text(app, SETTINGS_FILE)) return false;
    Ffkv kv;
    ffkv_parse(&kv, app->file_buf);
    if(!ffkv_check_header(&kv, SETTINGS_FILETYPE, FILE_VERSION)) return false;
    const char* mock = ffkv_get(&kv, "Mock_backend");
    app->settings.mock = APP_HAS_MOCK && mock && strcmp(mock, "yes") == 0;
    const char* dk = ffkv_get(&kv, "DefaultKey");
    if(dk && key_name_valid(dk))
        snprintf(app->settings.default_key, sizeof(app->settings.default_key), "%s", dk);
    const char* rn = ffkv_get(&kv, "Remember_notice");
    app->settings.remember_notice_seen = rn && strcmp(rn, "shown") == 0;
    const char* ex = ffkv_get(&kv, "Examples");
    app->settings.examples_seeded = ex && strcmp(ex, "seeded") == 0;
    return true;
}

bool store_load_settings(SshRunnerApp* app) {
    file_buf_acquire(app);
    bool r = store_load_settings_body(app);
    file_buf_release(app);
    return r;
}

static bool store_save_settings_body(SshRunnerApp* app) {
    FfkvWriter w;
    ffkv_writer_init(&w, app->file_buf, FILE_BUF_SIZE);
    ffkv_write_header(&w, SETTINGS_FILETYPE, FILE_VERSION);
    ffkv_write(&w, "Mock_backend", app->settings.mock ? "yes" : "no");
    if(app->settings.default_key[0]) ffkv_write(&w, "DefaultKey", app->settings.default_key);
    if(app->settings.remember_notice_seen) ffkv_write(&w, "Remember_notice", "shown");
    if(app->settings.examples_seeded) ffkv_write(&w, "Examples", "seeded");
    return !w.err && write_text(app, SETTINGS_FILE, app->file_buf);
}

bool store_save_settings(SshRunnerApp* app) {
    file_buf_acquire(app);
    bool r = store_save_settings_body(app);
    file_buf_release(app);
    return r;
}

static void host_path(char* buf, size_t size, const char* alias) {
    snprintf(buf, size, "%s/%s.conf", HOSTS_DIR, alias);
}

void store_load_hosts(SshRunnerApp* app) {
    app->host_count = 0;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, HOSTS_DIR)) {
        FileInfo info;
        char name[64];
        while(app->host_count < MAX_HOSTS && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            size_t n = strlen(name);
            if(n < 6 || strcmp(name + n - 5, ".conf") != 0) continue;
            name[n - 5] = '\0';
            if(!alias_valid(name)) continue;
            snprintf(app->host_aliases[app->host_count], PROFILE_ALIAS_MAX + 1, "%s", name);
            app->host_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    cstr_sort(&app->host_aliases[0][0], app->host_count, sizeof(app->host_aliases[0]));
}

bool store_profile_exists(SshRunnerApp* app, const char* alias) {
    char path[96];
    host_path(path, sizeof(path), alias);
    return storage_file_exists(app->storage, path);
}

static bool store_load_profile_body(SshRunnerApp* app, const char* alias, HostProfile* out) {
    char path[96];
    host_path(path, sizeof(path), alias);
    if(!read_text(app, path)) return false;
    bool ok = profile_from_text(app->file_buf, out);
    /* the file name is authoritative for the alias */
    if(ok && strcmp(out->alias, alias) != 0) snprintf(out->alias, sizeof(out->alias), "%s", alias);
    if(!ok) FURI_LOG_W(TAG, "bad host file: %s", path);
    return ok;
}

bool store_load_profile(SshRunnerApp* app, const char* alias, HostProfile* out) {
    file_buf_acquire(app);
    bool r = store_load_profile_body(app, alias, out);
    file_buf_release(app);
    return r;
}

static bool store_save_profile_body(SshRunnerApp* app, const HostProfile* p) {
    if(!alias_valid(p->alias)) return false;
    if(!profile_to_text(p, app->file_buf, FILE_BUF_SIZE)) return false;
    storage_simply_mkdir(app->storage, HOSTS_DIR);
    char path[96];
    host_path(path, sizeof(path), p->alias);
    return write_text(app, path, app->file_buf);
}

bool store_save_profile(SshRunnerApp* app, const HostProfile* p) {
    file_buf_acquire(app);
    bool r = store_save_profile_body(app, p);
    file_buf_release(app);
    return r;
}

bool store_delete_profile(SshRunnerApp* app, const char* alias) {
    char path[96];
    host_path(path, sizeof(path), alias);
    return storage_simply_remove(app->storage, path);
}

static void store_load_recent_body(SshRunnerApp* app) {
    recent_init(&app->recent);
    if(read_text(app, RECENT_FILE)) recent_from_text(app->file_buf, &app->recent);
}

void store_load_recent(SshRunnerApp* app) {
    file_buf_acquire(app);
    store_load_recent_body(app);
    file_buf_release(app);
}

static bool store_save_recent_body(SshRunnerApp* app) {
    if(!recent_to_text(&app->recent, app->file_buf, FILE_BUF_SIZE)) return false;
    return write_text(app, RECENT_FILE, app->file_buf);
}

bool store_save_recent(SshRunnerApp* app) {
    file_buf_acquire(app);
    bool r = store_save_recent_body(app);
    file_buf_release(app);
    return r;
}

/* ---------------- built-in examples ---------------- */

#define EXAMPLES_COMMANDS APP_ASSETS_PATH("examples/commands.conf")
#define EXAMPLES_SCRIPTS  APP_ASSETS_PATH("examples/scripts")

static const char* const example_scripts[] = {"sysinfo.sh"};

/* copies a file that is not there yet, as it is (an example the user edited is never replaced) */
static bool copy_if_missing(SshRunnerApp* app, const char* from, const char* to) {
    if(storage_file_exists(app->storage, to) || !read_text(app, from)) return false;
    return write_text(app, to, app->file_buf);
}

ExamplesResult store_examples(SshRunnerApp* app, bool first_run) {
    ExamplesResult r = {0};
    file_buf_acquire(app);
    r.available = storage_file_exists(app->storage, EXAMPLES_COMMANDS);
    for(size_t i = 0; i < COUNT_OF(example_scripts); i++) {
        char from[96], to[96];
        snprintf(from, sizeof(from), "%s/%s", EXAMPLES_SCRIPTS, example_scripts[i]);
        snprintf(to, sizeof(to), "%s/%s", SCRIPTS_DIR, example_scripts[i]);
        if(copy_if_missing(app, from, to)) r.scripts++;
    }
    if(!storage_file_exists(app->storage, COMMANDS_FILE)) {
        if(copy_if_missing(app, EXAMPLES_COMMANDS, COMMANDS_FILE))
            r.commands = commands_text_count(app->file_buf);
    } else if(!first_run && read_text(app, EXAMPLES_COMMANDS)) {
        /* the user's file stays as it is; the missing examples are appended to it */
        size_t n = strlen(app->file_buf) + 1;
        char* ex = malloc(n);
        memcpy(ex, app->file_buf, n);
        char* add = malloc(n + 64);
        if(read_text(app, COMMANDS_FILE)) {
            unsigned added = 0, no_room = 0;
            commands_missing(app->file_buf, ex, add, n + 64, &added, &no_room);
            size_t cur = strlen(app->file_buf);
            bool nl = cur && app->file_buf[cur - 1] != '\n';
            if(added) {
                File* f = storage_file_alloc(app->storage);
                if(storage_file_open(f, COMMANDS_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
                    if(nl) storage_file_write(f, "\n", 1);
                    if(storage_file_write(f, add, strlen(add)) == strlen(add)) r.commands = added;
                }
                storage_file_close(f);
                storage_file_free(f);
            }
            r.no_room = no_room;
        }
        free(add);
        free(ex);
    }
    file_buf_release(app);
    return r;
}

static void store_load_commands_body(SshRunnerApp* app) {
    commands_init(app->commands);
    if(read_text(app, COMMANDS_FILE)) {
        commands_from_text(app->file_buf, app->commands);
        return;
    }
    /* First run: a small example file the user can edit with qFlipper. */
    commands_add(app->commands, "uptime");
    commands_add(app->commands, "df -h");
    if(commands_to_text(app->commands, app->file_buf, FILE_BUF_SIZE))
        write_text(app, COMMANDS_FILE, app->file_buf);
}

void store_load_commands(SshRunnerApp* app) {
    file_buf_acquire(app);
    store_load_commands_body(app);
    file_buf_release(app);
}

typedef struct {
    File* f;
    bool ok;
} FileSink;

static void file_sink(void* ctx, const char* data, size_t len) {
    FileSink* fs = ctx;
    if(fs->ok && len) fs->ok = storage_file_write(fs->f, data, len) == len;
}

bool store_save_output(SshRunnerApp* app, char* path_out, size_t path_size) {
    storage_simply_mkdir(app->storage, LOGS_DIR);
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char id[PROTO_STR_PROFILE + 1];
    if(app->profile.alias[0])
        snprintf(id, sizeof(id), "%s", app->profile.alias);
    else
        alias_suggest(app->profile.target.host, id, sizeof(id));
    /* logs/<alias>/YYMMDD-HHMMSS.log: a 17-character name fits one FontKeyboard line */
    snprintf(path_out, path_size, "%s/%s", LOGS_DIR, id);
    storage_simply_mkdir(app->storage, path_out);
    snprintf(
        path_out,
        path_size,
        "%s/%s/%02u%02u%02u-%02u%02u%02u.log",
        LOGS_DIR,
        id,
        dt.year % 100u,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
    /* Streamed straight from the session buffer to the file: no copy of the output in
     * RAM (a FAP has little heap; an 8 KB copy here crashed the Flipper out of memory). */
    char t[96], line[160];
    target_format(&app->profile.target, t, sizeof(t));
    bool ok = false;
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path_out, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        Session* se = app->session;
        int n = snprintf(
            line, sizeof(line), "# %s\n%s %s\n", t, app->run_is_script ? "#" : "$", app->run_title);
        FileSink fs = {f, n > 0 && storage_file_write(f, line, (size_t)n) == (size_t)n};
        outbuf_emit(&se->out, file_sink, &fs);
        if(se->have_done) {
            char ex[48];
            format_exit_line(
                ex, sizeof(ex), se->done.exit_code, se->done.flags, se->done.duration_ms);
            n = snprintf(line, sizeof(line), "\n[%s]\n", ex);
        } else {
            n = snprintf(line, sizeof(line), "\n[running...]\n");
        }
        ok = fs.ok && n > 0 && storage_file_write(f, line, (size_t)n) == (size_t)n;
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

/* ---------------- keys/ ---------------- */

static void key_path(char* buf, size_t size, const char* name, bool pub) {
    snprintf(buf, size, "%s/%s%s", KEYS_DIR, name, pub ? ".pub" : "");
}

/* Up to `cap` bytes of a file into `buf`; returns the count (0 if it cannot be read). */
static size_t read_head(SshRunnerApp* app, const char* path, char* buf, size_t cap) {
    size_t n = 0;
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING))
        n = storage_file_read(f, buf, cap);
    storage_file_close(f);
    storage_file_free(f);
    return n;
}

static bool store_keys_scan_body(SshRunnerApp* app, KeyList* out) {
    keylist_init(out);
    storage_simply_mkdir(app->storage, KEYS_DIR);
    uint8_t* scratch = malloc(KEYDIR_SCRATCH);
    char* pub = KEY_LINE_BUF(app);
    File* dir = storage_file_alloc(app->storage);
    bool ok = storage_dir_open(dir, KEYS_DIR);
    if(ok) {
        FileInfo info;
        char name[64], path[96];
        while(out->count < KEYS_MAX && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(!keydir_is_candidate(name, file_info_is_dir(&info), info.size)) continue;
            key_path(path, sizeof(path), name, false);
            /* the head of a private key passes through file_buf and is wiped below */
            size_t hn = read_head(app, path, app->file_buf, KEYFMT_SNIFF_BYTES);
            key_path(path, sizeof(path), name, true);
            size_t pn = read_head(app, path, pub, KEY_PUB_LINE_MAX);
            pub[pn] = '\0';
            KeyEntry e;
            keydir_inspect(
                &e, name, app->file_buf, hn, pn ? pub : NULL, pn, scratch, KEYDIR_SCRATCH);
            memset(app->file_buf, 0, KEYFMT_SNIFF_BYTES);
            keylist_add(out, &e);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    memset(pub, 0, KEY_PUB_LINE_MAX + 1);
    free(scratch);
    return ok;
}

bool store_keys_scan(SshRunnerApp* app, KeyList* out) {
    file_buf_acquire(app);
    bool r = store_keys_scan_body(app, out);
    file_buf_release(app);
    return r;
}

bool store_key_exists(SshRunnerApp* app, const char* name) {
    char path[96];
    key_path(path, sizeof(path), name, false);
    return storage_file_exists(app->storage, path);
}

File* store_key_open(SshRunnerApp* app, const char* name, uint32_t* size) {
    char path[96];
    key_path(path, sizeof(path), name, false);
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return NULL;
    }
    *size = (uint32_t)storage_file_size(f);
    return f;
}

/* CRC-32 of a file, to prove what was written is what came over the link. */
static bool file_crc(SshRunnerApp* app, const char* path, uint32_t* crc, uint32_t* len) {
    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING);
    *crc = 0;
    *len = 0;
    if(ok) {
        uint8_t buf[256];
        size_t n;
        while((n = storage_file_read(f, buf, sizeof(buf))) > 0) {
            *crc = proto_crc32(buf, n, *crc);
            *len += (uint32_t)n;
        }
        memset(buf, 0, sizeof(buf));
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

bool store_key_write(
    SshRunnerApp* app,
    const char* name,
    const uint8_t* data,
    uint32_t len,
    uint32_t crc32) {
    storage_simply_mkdir(app->storage, KEYS_DIR);
    char path[96];
    key_path(path, sizeof(path), name, false);
    File* f = storage_file_alloc(app->storage);
    /* never over an existing key */
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_NEW) &&
              storage_file_write(f, data, len) == len;
    storage_file_close(f);
    storage_file_free(f);
    uint32_t back_crc, back_len;
    ok = ok && file_crc(app, path, &back_crc, &back_len) && back_len == len && back_crc == crc32;
    if(!ok) {
        FURI_LOG_E(TAG, "key write failed: %s", name);
        storage_simply_remove(app->storage, path);
    }
    return ok;
}

bool store_key_write_pub(SshRunnerApp* app, const char* name, const char* line) {
    char path[96];
    key_path(path, sizeof(path), name, true);
    /* `line` may sit in file_buf itself (KEY_LINE_BUF): written as it is */
    size_t n = strlen(line);
    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(f, line, n) == n && storage_file_write(f, "\n", 1) == 1;
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

size_t store_key_read_pub(SshRunnerApp* app, const char* name, char* out, size_t size) {
    char path[96];
    key_path(path, sizeof(path), name, true);
    size_t n = read_head(app, path, out, size - 1);
    out[n] = '\0';
    while(n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' '))
        out[--n] = '\0';
    return n;
}

/* The public key line from the file itself (an OpenSSH container's public section). */
static size_t
    store_key_derive_pub_body(SshRunnerApp* app, const char* name, char* out, size_t size) {
    char path[96];
    key_path(path, sizeof(path), name, false);
    uint8_t* scratch = malloc(KEYDIR_SCRATCH);
    size_t hn = read_head(app, path, app->file_buf, KEYFMT_SNIFF_BYTES);
    KeyInfo ki;
    keyfmt_sniff(app->file_buf, hn, &ki, scratch, KEYDIR_SCRATCH);
    size_t n = ki.pub ? keyfmt_pub_line(ki.pub, ki.pub_len, name, out, size) : 0;
    memset(app->file_buf, 0, KEYFMT_SNIFF_BYTES);
    memset(scratch, 0, KEYDIR_SCRATCH);
    free(scratch);
    return n;
}

size_t store_key_derive_pub(SshRunnerApp* app, const char* name, char* out, size_t size) {
    file_buf_acquire(app);
    size_t r = store_key_derive_pub_body(app, name, out, size);
    file_buf_release(app);
    return r;
}

/* "file:SHA256:..." for a key without a known public key (the ESP32 uses the same id). */
bool store_key_file_id(SshRunnerApp* app, const char* name, char* out, size_t size) {
    char path[96];
    key_path(path, sizeof(path), name, false);
    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) {
        KeyfmtSha256 s;
        uint8_t buf[256], d[32];
        size_t n;
        keyfmt_sha256_init(&s);
        while((n = storage_file_read(f, buf, sizeof(buf))) > 0)
            keyfmt_sha256_update(&s, buf, n);
        keyfmt_sha256_final(&s, d);
        memset(buf, 0, sizeof(buf));
        char id[KEYFMT_FP_SIZE + 5];
        keyfmt_file_id(d, id);
        snprintf(out, size, "%s", id);
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

/* Hosts that use key `name`: their aliases into `aliases` (up to `max`), the count. */
unsigned store_key_users(
    SshRunnerApp* app,
    const char* name,
    char (*aliases)[PROFILE_ALIAS_MAX + 1],
    unsigned max) {
    store_load_hosts(app);
    unsigned n = 0;
    HostProfile* p = malloc(sizeof(HostProfile));
    if(!p) return 0;
    for(uint8_t i = 0; i < app->host_count; i++) {
        if(!store_load_profile(app, app->host_aliases[i], p)) continue;
        if(!key_used_by(p, name, app->settings.default_key)) continue;
        if(n < max) snprintf(aliases[n], PROFILE_ALIAS_MAX + 1, "%s", p->alias);
        n++;
    }
    free(p);
    return n;
}

/* Rename both files and the Key: of every host that names it; the default follows. */
bool store_key_rename(SshRunnerApp* app, const char* old_name, const char* new_name) {
    char from[96], to[96];
    key_path(from, sizeof(from), old_name, false);
    key_path(to, sizeof(to), new_name, false);
    if(storage_file_exists(app->storage, to)) return false;
    if(storage_common_rename(app->storage, from, to) != FSE_OK) return false;
    key_path(from, sizeof(from), old_name, true);
    key_path(to, sizeof(to), new_name, true);
    if(storage_file_exists(app->storage, from)) storage_common_rename(app->storage, from, to);
    store_load_hosts(app);
    HostProfile* p = malloc(sizeof(HostProfile));
    if(p) {
        for(uint8_t i = 0; i < app->host_count; i++)
            if(store_load_profile(app, app->host_aliases[i], p) &&
               profile_rename_key(p, old_name, new_name))
                store_save_profile(app, p);
        free(p);
    }
    if(strcmp(app->settings.default_key, old_name) == 0) {
        snprintf(app->settings.default_key, sizeof(app->settings.default_key), "%s", new_name);
        store_save_settings(app);
    }
    return true;
}

/* Delete both files; a default is cleared; hosts that used it switch to the default key,
 * or to password (profile_key_deleted). *switched counts them, *to says to what. */
bool store_key_delete(SshRunnerApp* app, const char* name, unsigned* switched, uint8_t* to) {
    char path[96];
    key_path(path, sizeof(path), name, false);
    bool ok = storage_simply_remove(app->storage, path);
    key_path(path, sizeof(path), name, true);
    storage_simply_remove(app->storage, path);
    if(strcmp(app->settings.default_key, name) == 0) {
        app->settings.default_key[0] = '\0';
        store_save_settings(app);
    }
    *switched = 0;
    *to = app->settings.default_key[0] ? ProfileAuthDefault : ProfileAuthPassword;
    store_load_hosts(app);
    HostProfile* p = malloc(sizeof(HostProfile));
    if(p) {
        for(uint8_t i = 0; i < app->host_count; i++)
            if(store_load_profile(app, app->host_aliases[i], p) &&
               profile_key_deleted(p, name, app->settings.default_key) &&
               store_save_profile(app, p))
                (*switched)++;
        free(p);
    }
    return ok;
}
