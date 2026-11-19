#include "updi_app_i.h"

#include "hex/intel_hex.h"
#include "updi/updi_constants.h"

#include <storage/storage.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* fuse_names_dx[12] = {
    "WDTCFG",
    "BODCFG",
    "OSCCFG",
    "Reserved",
    "Reserved",
    "SYSCFG0",
    "SYSCFG1",
    "CODESIZE",
    "BOOTSIZE",
    "Reserved",
    "PDICFG L",
    "PDICFG H",
};

static void updi_result(UpdiApp* app, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->result, sizeof(app->result), fmt, args);
    va_end(args);
}

static bool updi_report(UpdiApp* app, const char* status, uint8_t percent) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->status, sizeof(app->status), "%s", status);
    app->percent = percent;
    furi_mutex_release(app->mutex);
    view_dispatcher_send_custom_event(app->view_dispatcher, UpdiEventProgress);
    return !app->cancel;
}

static void updi_finish(UpdiApp* app) {
    view_dispatcher_send_custom_event(app->view_dispatcher, UpdiEventFinished);
}

static bool updi_require_known(UpdiApp* app, const UpdiTargetInfo* target, bool erased) {
    if(target->known && target->profile != NULL) return true;
    if(erased) {
        updi_result(
            app,
            "Chip was erased, but signature %02X %02X %02X is unknown. Nothing was written.",
            target->signature[0],
            target->signature[1],
            target->signature[2]);
    } else {
        updi_result(
            app,
            "%s\nSig %02X %02X %02X\nUnknown part.\nFlash and dump are refused.",
            target->name,
            target->signature[0],
            target->signature[1],
            target->signature[2]);
    }
    return false;
}

typedef enum {
    HexPassCount,
    HexPassWrite,
    HexPassVerify,
} HexPass;

typedef struct {
    UpdiApp* app;
    UpdiSession* session;
    const UpdiDeviceProfile* profile;
    HexPass pass;
    uint32_t pages;
    bool failed;
    bool has_mismatch;
    uint32_t mismatch;
    uint8_t file_byte;
    uint8_t chip_byte;
} HexJob;

static bool hex_on_page(uint32_t address, const uint8_t* data, uint16_t page_size, void* context) {
    HexJob* job = context;
    UNUSED(page_size);
    if(job->app->cancel) return false;

    if(job->pass == HexPassCount) {
        job->pages++;
        return true;
    }

    if(job->pass == HexPassWrite) {
        if(!updi_session_write_page(job->session, address, data)) {
            job->failed = true;
            return false;
        }
        job->pages++;
        return true;
    }

    uint8_t chip[INTEL_HEX_MAX_PAGE_SIZE];
    if(!updi_session_read_flash(job->session, address, chip, job->profile->flash_page)) {
        job->failed = true;
        return false;
    }
    for(size_t i = 0; i < job->profile->flash_page; i++) {
        if(chip[i] != data[i]) {
            job->failed = true;
            job->has_mismatch = true;
            job->mismatch = address + (uint32_t)i;
            job->file_byte = data[i];
            job->chip_byte = chip[i];
            return false;
        }
    }
    job->pages++;
    return true;
}

static bool hex_pass(
    UpdiApp* app,
    File* file,
    uint64_t file_size,
    HexJob* job,
    HexPass pass,
    uint8_t percent_from,
    uint8_t percent_to,
    char* parse_error,
    size_t parse_error_size) {
    const char* label = "Checking HEX";
    if(pass == HexPassWrite) label = "Writing flash";
    if(pass == HexPassVerify) label = "Verifying";

    if(!storage_file_seek(file, 0, true)) {
        snprintf(parse_error, parse_error_size, "Could not rewind HEX file");
        return false;
    }

    job->pass = pass;
    job->pages = 0;
    job->failed = false;

    IntelHexParser* parser = intel_hex_parser_alloc(
        job->profile->flash_size, job->profile->flash_page, hex_on_page, job);
    if(parser == NULL) {
        snprintf(parse_error, parse_error_size, "Out of memory");
        return false;
    }

    char line[600];
    size_t line_len = 0;
    uint8_t chunk[128];
    uint64_t pos = 0;
    bool ok = true;

    while(ok) {
        size_t nread = storage_file_read(file, chunk, sizeof(chunk));
        pos += nread;
        for(size_t i = 0; i < nread && ok; i++) {
            char c = (char)chunk[i];
            if(c == '\n' || c == '\r') {
                if(line_len == 0) continue;
                line[line_len] = '\0';
                line_len = 0;
                if(!intel_hex_parser_feed(parser, line, parse_error, parse_error_size)) ok = false;
            } else if(line_len + 1 < sizeof(line)) {
                line[line_len++] = c;
            } else {
                snprintf(parse_error, parse_error_size, "HEX line too long");
                ok = false;
            }
        }

        uint8_t percent = percent_from;
        if(file_size > 0) {
            percent = (uint8_t)(percent_from + ((percent_to - percent_from) * pos) / file_size);
        }
        if(!updi_report(app, label, percent)) {
            snprintf(parse_error, parse_error_size, "Cancelled");
            ok = false;
        }
        if(nread == 0) break;
    }

    if(ok && line_len > 0) {
        line[line_len] = '\0';
        if(!intel_hex_parser_feed(parser, line, parse_error, parse_error_size)) ok = false;
    }
    if(ok && !intel_hex_parser_finish(parser, parse_error, parse_error_size)) ok = false;

    intel_hex_parser_free(parser);
    return ok;
}

typedef struct {
    UpdiApp* app;
    UpdiSession* session;
    const UpdiDeviceProfile* profile;
    bool failed;
    uint32_t pages;
} EepromJob;

static bool
    eeprom_on_page(uint32_t address, const uint8_t* data, uint16_t page_size, void* context) {
    EepromJob* job = context;
    if(job->app->cancel) return false;
    size_t len = page_size;
    if(address >= job->profile->eeprom_size) return true;
    if(address + len > job->profile->eeprom_size) {
        len = job->profile->eeprom_size - address;
    }
    if(!updi_session_write_eeprom(job->session, address, data, len)) {
        job->failed = true;
        return false;
    }
    job->pages++;
    return true;
}

static bool dump_write_line(const char* line, void* context) {
    File* file = context;
    size_t len = strlen(line);
    if(storage_file_write(file, line, len) != len) return false;
    return storage_file_write(file, "\n", 1) == 1;
}

static bool dump_memory_hex(
    UpdiApp* app,
    UpdiSession* session,
    Storage* storage,
    const char* path,
    uint32_t size,
    bool eeprom,
    const char* label) {
    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!opened) {
        updi_result(app, "Could not create\n%s", path);
        storage_file_free(file);
        return false;
    }

    IntelHexWriter* writer = intel_hex_writer_alloc(dump_write_line, file);
    char error[96] = {0};
    bool ok = writer != NULL;
    if(!ok) snprintf(error, sizeof(error), "Out of memory");

    uint8_t buffer[256];
    for(uint32_t address = 0; ok && address < size; address += sizeof(buffer)) {
        if(app->cancel) {
            snprintf(error, sizeof(error), "Cancelled");
            ok = false;
            break;
        }
        size_t chunk = sizeof(buffer);
        if(address + chunk > size) chunk = size - address;
        if(eeprom) {
            if(!updi_session_read_eeprom(session, address, buffer, chunk)) {
                snprintf(error, sizeof(error), "%s", updi_session_error(session));
                ok = false;
                break;
            }
        } else if(!updi_session_read_flash(session, address, buffer, chunk)) {
            snprintf(error, sizeof(error), "%s", updi_session_error(session));
            ok = false;
            break;
        }
        if(!intel_hex_writer_feed(writer, address, buffer, chunk, error, sizeof(error))) {
            ok = false;
            break;
        }
        uint8_t percent = size ? (uint8_t)((address * 100u) / size) : 100;
        if(!updi_report(app, label, percent)) {
            snprintf(error, sizeof(error), "Cancelled");
            ok = false;
            break;
        }
    }
    if(ok && !intel_hex_writer_finish(writer, error, sizeof(error))) ok = false;

    intel_hex_writer_free(writer);
    storage_file_close(file);
    storage_file_free(file);

    if(!ok) {
        updi_result(app, "%s", error[0] ? error : "Dump failed");
    }
    return ok;
}

static bool feed_hex_file(
    File* file,
    IntelHexParser* parser,
    UpdiApp* app,
    const char* label,
    char* parse_error,
    size_t parse_error_size) {
    if(!storage_file_seek(file, 0, true)) {
        snprintf(parse_error, parse_error_size, "Could not rewind HEX file");
        return false;
    }
    uint64_t file_size = storage_file_size(file);
    char line[600];
    size_t line_len = 0;
    uint8_t chunk[128];
    uint64_t pos = 0;
    bool ok = true;
    while(ok) {
        size_t nread = storage_file_read(file, chunk, sizeof(chunk));
        pos += nread;
        for(size_t i = 0; i < nread && ok; i++) {
            char c = (char)chunk[i];
            if(c == '\n' || c == '\r') {
                if(line_len == 0) continue;
                line[line_len] = '\0';
                line_len = 0;
                if(!intel_hex_parser_feed(parser, line, parse_error, parse_error_size)) ok = false;
            } else if(line_len + 1 < sizeof(line)) {
                line[line_len++] = c;
            } else {
                snprintf(parse_error, parse_error_size, "HEX line too long");
                ok = false;
            }
        }
        uint8_t percent = 10;
        if(file_size > 0) percent = (uint8_t)(10 + (80 * pos) / file_size);
        if(!updi_report(app, label, percent)) {
            snprintf(parse_error, parse_error_size, "Cancelled");
            ok = false;
        }
        if(nread == 0) break;
    }
    if(ok && line_len > 0) {
        line[line_len] = '\0';
        if(!intel_hex_parser_feed(parser, line, parse_error, parse_error_size)) ok = false;
    }
    if(ok && !intel_hex_parser_finish(parser, parse_error, parse_error_size)) ok = false;
    return ok;
}

static void updi_worker_detect(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }

    UpdiConnectStatus status = updi_session_connect(session);
    const UpdiTargetInfo* target = updi_session_target(session);
    if(status == UpdiConnectFail) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }
    if(status == UpdiConnectLocked) {
        updi_result(
            app,
            "Device is locked.\nNVM %s\nSIB: %s\n\nFlash chip-erases to unlock.",
            target->nvm,
            target->sib);
        return;
    }

    const UpdiDeviceProfile* profile = target->profile;
    if(profile != NULL) {
        updi_result(
            app,
            "%s\nSig %02X %02X %02X\nNVM %s\nFlash %lu KB page %u\nEEPROM %lu\nBaud %lu\nSIB: %s",
            target->name,
            target->signature[0],
            target->signature[1],
            target->signature[2],
            target->nvm,
            (unsigned long)(profile->flash_size / 1024u),
            profile->flash_page,
            (unsigned long)profile->eeprom_size,
            (unsigned long)updi_session_baud(session),
            target->sib);
    } else {
        updi_result(
            app,
            "%s\nSig %02X %02X %02X\nNVM %s\nBaud %lu\nSIB: %s",
            target->name,
            target->signature[0],
            target->signature[1],
            target->signature[2],
            target->nvm,
            (unsigned long)updi_session_baud(session),
            target->sib);
    }
}

static void updi_worker_fuses(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }
    UpdiConnectStatus status = updi_session_connect(session);
    const UpdiTargetInfo* target = updi_session_target(session);
    if(status == UpdiConnectFail) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }
    if(status == UpdiConnectLocked) {
        updi_result(app, "Device is locked.\nFuses are hidden until a chip erase.");
        return;
    }
    if(!updi_require_known(app, target, false)) return;

    uint8_t fuses[UPDI_FUSE_MAX];
    size_t count = 0;
    if(!updi_session_read_fuses(session, fuses, sizeof(fuses), &count)) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }

    char body[512];
    size_t used = (size_t)snprintf(
        body,
        sizeof(body),
        "%s\nSig %02X %02X %02X\n",
        target->name,
        target->signature[0],
        target->signature[1],
        target->signature[2]);
    for(size_t i = 0; i < count && used < sizeof(body); i++) {
        if(target->profile->nvm_family == UpdiNvmP2 && i < 12) {
            used += (size_t)snprintf(
                body + used, sizeof(body) - used, "%s %02X\n", fuse_names_dx[i], fuses[i]);
        } else {
            used += (size_t)snprintf(
                body + used, sizeof(body) - used, "FUSE%u %02X\n", (unsigned)i, fuses[i]);
        }
    }
    updi_result(app, "%s", body);
}

static void updi_worker_write_fuse(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }
    UpdiConnectStatus status = updi_session_connect(session);
    if(status == UpdiConnectFail) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }
    if(status == UpdiConnectLocked) {
        updi_result(app, "Device is locked.");
        return;
    }
    if(!updi_require_known(app, updi_session_target(session), false)) return;

    if(!updi_report(app, "Writing fuse", 40)) {
        updi_result(app, "Cancelled");
        return;
    }
    if(!updi_session_write_fuse(session, app->fuse_index, app->fuse_value)) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }
    updi_result(
        app,
        "Wrote FUSE%u = %02X\non %s",
        app->fuse_index,
        app->fuse_value,
        updi_session_target(session)->name);
}

static void updi_worker_dump(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }
    UpdiConnectStatus status = updi_session_connect(session);
    if(status == UpdiConnectFail) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }
    if(status == UpdiConnectLocked) {
        updi_result(app, "Device is locked.\nDump needs a chip erase, which Flash can do.");
        return;
    }
    if(!updi_require_known(app, updi_session_target(session), false)) return;

    const UpdiDeviceProfile* profile = updi_session_target(session)->profile;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, UPDI_APPS_DATA_DIR);

    char dir[96];
    snprintf(dir, sizeof(dir), "%s/%s", UPDI_APPS_DATA_DIR, app->dump_name);
    storage_simply_mkdir(storage, dir);

    char flash_path[128];
    char eeprom_path[128];
    char fuses_path[128];
    snprintf(flash_path, sizeof(flash_path), "%s/flash.hex", dir);
    snprintf(eeprom_path, sizeof(eeprom_path), "%s/eeprom.hex", dir);
    snprintf(fuses_path, sizeof(fuses_path), "%s/fuses.txt", dir);

    if(!dump_memory_hex(
           app, session, storage, flash_path, profile->flash_size, false, "Dumping flash")) {
        furi_record_close(RECORD_STORAGE);
        return;
    }
    if(!dump_memory_hex(
           app, session, storage, eeprom_path, profile->eeprom_size, true, "Dumping EEPROM")) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint8_t fuses[UPDI_FUSE_MAX];
    size_t count = 0;
    if(!updi_session_read_fuses(session, fuses, sizeof(fuses), &count)) {
        updi_result(app, "%s", updi_session_error(session));
        furi_record_close(RECORD_STORAGE);
        return;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, fuses_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        updi_result(app, "Could not create fuses.txt");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    char line[48];
    for(size_t i = 0; i < count; i++) {
        snprintf(line, sizeof(line), "FUSE%u=%02X\n", (unsigned)i, fuses[i]);
        storage_file_write(file, line, strlen(line));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    updi_result(
        app, "Dumped %s\nto\n%s\nflash+eeprom+fuses", updi_session_target(session)->name, dir);
}

static void updi_worker_eeprom_read(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }
    UpdiConnectStatus status = updi_session_connect(session);
    if(status != UpdiConnectOk) {
        updi_result(
            app, status == UpdiConnectLocked ? "Device is locked." : updi_session_error(session));
        return;
    }
    if(!updi_require_known(app, updi_session_target(session), false)) return;

    const UpdiDeviceProfile* profile = updi_session_target(session)->profile;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, UPDI_APPS_DATA_DIR);
    char path[96];
    snprintf(path, sizeof(path), "%s/eeprom.hex", UPDI_APPS_DATA_DIR);
    bool ok =
        dump_memory_hex(app, session, storage, path, profile->eeprom_size, true, "Reading EEPROM");
    furi_record_close(RECORD_STORAGE);
    if(ok) updi_result(app, "EEPROM dumped to\n%s", path);
}

static void updi_worker_eeprom_write(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }
    UpdiConnectStatus status = updi_session_connect(session);
    if(status != UpdiConnectOk) {
        updi_result(
            app, status == UpdiConnectLocked ? "Device is locked." : updi_session_error(session));
        return;
    }
    if(!updi_require_known(app, updi_session_target(session), false)) return;

    const UpdiDeviceProfile* profile = updi_session_target(session)->profile;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    const char* path = furi_string_get_cstr(app->eeprom_path);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        updi_result(app, "Could not open EEPROM HEX");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    EepromJob job = {.app = app, .session = session, .profile = profile};
    char parse_error[96] = {0};
    IntelHexParser* parser =
        intel_hex_parser_alloc(profile->eeprom_size, profile->eeprom_page, eeprom_on_page, &job);
    bool ok = parser != NULL;
    if(!ok) snprintf(parse_error, sizeof(parse_error), "Out of memory");
    if(ok) {
        ok = feed_hex_file(file, parser, app, "Writing EEPROM", parse_error, sizeof(parse_error));
        if(!ok && job.failed) {
            snprintf(parse_error, sizeof(parse_error), "%s", updi_session_error(session));
        }
    }
    intel_hex_parser_free(parser);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        updi_result(
            app, "Wrote EEPROM\n%lu pages\non %s", (unsigned long)job.pages, profile->name);
    } else if(app->cancel) {
        updi_result(app, "Cancelled");
    } else {
        updi_result(app, "%s", parse_error[0] ? parse_error : "EEPROM write failed");
    }
}

static void updi_worker_flash(UpdiApp* app, UpdiSession* session) {
    if(!updi_report(app, "Connecting", 0)) {
        updi_result(app, "Cancelled");
        return;
    }

    bool erased_by_key = false;
    UpdiConnectStatus status = updi_session_connect(session);
    if(status == UpdiConnectFail) {
        updi_result(app, "%s", updi_session_error(session));
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    const char* path = furi_string_get_cstr(app->hex_path);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        updi_result(app, "Could not open HEX file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint64_t file_size = storage_file_size(file);
    HexJob* job = calloc(1, sizeof(HexJob));
    char parse_error[96] = {0};
    bool ok = job != NULL;
    if(!ok) snprintf(parse_error, sizeof(parse_error), "Out of memory");

    /* For locked chips, validate HEX against a temporary max size until unlocked. */
    const UpdiDeviceProfile* provisional = updi_session_target(session)->profile;
    if(ok && provisional == NULL && status != UpdiConnectLocked) {
        snprintf(parse_error, sizeof(parse_error), "Unknown device");
        ok = false;
    }

    if(ok && status == UpdiConnectLocked) {
        /* Count with max 128K/512 until we know the part after unlock. */
        static const UpdiDeviceProfile locked_probe = {
            .name = "locked",
            .flash_size = 128u * 1024u,
            .flash_page = 512u,
        };
        job->app = app;
        job->session = session;
        job->profile = &locked_probe;
        ok = hex_pass(
            app, file, file_size, job, HexPassCount, 8, 25, parse_error, sizeof(parse_error));
        if(ok && job->pages == 0) {
            snprintf(parse_error, sizeof(parse_error), "HEX file has no flash data");
            ok = false;
        }
        if(ok) {
            if(!updi_report(app, "Erasing lock", 28)) {
                snprintf(parse_error, sizeof(parse_error), "Cancelled");
                ok = false;
            } else if(
                !updi_session_unlock(session) || !updi_session_enter_progmode(session) ||
                !updi_session_read_signature(session)) {
                snprintf(parse_error, sizeof(parse_error), "%s", updi_session_error(session));
                ok = false;
            } else {
                erased_by_key = true;
            }
        }
    }

    const UpdiTargetInfo* target = updi_session_target(session);
    if(ok && !target->known) {
        ok = false;
        if(erased_by_key) {
            snprintf(
                parse_error,
                sizeof(parse_error),
                "Erased, unknown sig %02X%02X%02X",
                target->signature[0],
                target->signature[1],
                target->signature[2]);
        } else {
            snprintf(
                parse_error,
                sizeof(parse_error),
                "Unknown sig %02X%02X%02X",
                target->signature[0],
                target->signature[1],
                target->signature[2]);
        }
    }

    if(ok) {
        job->app = app;
        job->session = session;
        job->profile = target->profile;
        ok = hex_pass(
            app, file, file_size, job, HexPassCount, 8, 25, parse_error, sizeof(parse_error));
        if(ok && job->pages == 0) {
            snprintf(parse_error, sizeof(parse_error), "HEX file has no flash data");
            ok = false;
        }
    }

    uint32_t pages = ok ? job->pages : 0;

    if(ok) {
        if(!updi_report(app, "Erasing chip", 30)) {
            snprintf(parse_error, sizeof(parse_error), "Cancelled");
            ok = false;
        }
    }
    if(ok && !updi_session_chip_erase(session)) {
        snprintf(parse_error, sizeof(parse_error), "%s", updi_session_error(session));
        ok = false;
    }

    if(ok) {
        ok = hex_pass(
            app, file, file_size, job, HexPassWrite, 40, 75, parse_error, sizeof(parse_error));
        if(!ok && job->failed) {
            snprintf(parse_error, sizeof(parse_error), "%s", updi_session_error(session));
        }
    }

    if(ok) {
        ok = hex_pass(
            app, file, file_size, job, HexPassVerify, 75, 100, parse_error, sizeof(parse_error));
        if(!ok && job != NULL && job->has_mismatch) {
            snprintf(
                parse_error,
                sizeof(parse_error),
                "Verify mismatch at 0x%05lX file %02X chip %02X",
                (unsigned long)job->mismatch,
                job->file_byte,
                job->chip_byte);
        } else if(!ok && job != NULL && job->failed) {
            snprintf(parse_error, sizeof(parse_error), "%s", updi_session_error(session));
        }
    }

    if(ok) {
        updi_result(
            app,
            "Flashed %s\nPages: %lu\nVerified\nBaud %lu",
            target->name,
            (unsigned long)pages,
            (unsigned long)updi_session_baud(session));
    } else if(app->cancel) {
        updi_result(app, "Cancelled");
    } else {
        updi_result(app, "%s", parse_error[0] ? parse_error : "Flash failed");
    }

    free(job);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

int32_t updi_worker(void* context) {
    UpdiApp* app = context;
    UpdiSession* session = updi_session_alloc();
    if(session == NULL) {
        updi_result(app, "Out of memory");
        updi_finish(app);
        return 0;
    }
    updi_session_set_pins(session, app->pins);

    switch(app->op) {
    case UpdiOpDetect:
        updi_worker_detect(app, session);
        break;
    case UpdiOpFlash:
        updi_worker_flash(app, session);
        break;
    case UpdiOpDump:
        updi_worker_dump(app, session);
        break;
    case UpdiOpFuses:
        updi_worker_fuses(app, session);
        break;
    case UpdiOpReadEeprom:
        updi_worker_eeprom_read(app, session);
        break;
    case UpdiOpWriteEeprom:
        updi_worker_eeprom_write(app, session);
        break;
    case UpdiOpWriteFuse:
        updi_worker_write_fuse(app, session);
        break;
    }

    updi_session_disconnect(session);
    updi_session_free(session);
    updi_report(app, "Done", 100);
    updi_finish(app);
    return 0;
}
