/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_export.h"

#include "../emrtd_wipe.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>

#include "../emrtd.h"
#include "../crypto/emrtd_crypto.h"
#include "../protocol/emrtd_files.h"
#include "../protocol/emrtd_lds.h"
#include "../protocol/emrtd_mrz.h"
#include "../protocol/emrtd_security_info.h"

#define TAG "EmrtdExport"

/**
 * The export directory: the fixed prefix, the document number and a stamp.
 *
 * Kept separate from, and smaller than, the buffer a path inside it is built
 * in, so that appending a file name to it cannot be truncated.
 */
#define EMRTD_EXPORT_DIR_SIZE    72
/** A path to one file in the export directory. */
#define EMRTD_EXPORT_PATH_SIZE   128
/** One formatted report line; the report is written a line at a time. */
#define EMRTD_EXPORT_LINE_SIZE   192
/** Bytes hex encoded per storage_file_write() while tracing. */
#define EMRTD_EXPORT_TRACE_SLICE 32
/** One '#' line of the trace. The card description is the longest of them. */
#define EMRTD_EXPORT_NOTE_SIZE   160

/**
 * The grammar version written into the head of every trace.
 *
 * Anything that reads a trace reads this first, so it moves whenever a line
 * changes shape - a new note is not a change of shape, a new kind of line is.
 */
#define EMRTD_TRACE_FORMAT 2u

struct EmrtdExport {
    Storage* storage;
    File* file; /**< The raw data group currently being streamed. */
    File* image; /**< The facial image lifted out of DG2. */
    File* trace; /**< trace.txt, open for the whole run. */
    char path[EMRTD_EXPORT_DIR_SIZE];
    bool failed;
    bool want_trace;
    bool image_armed;
    size_t image_offset; /**< Where the image starts within the open file. */
    size_t offset; /**< Bytes already appended to the open file. */
};

static const char emrtd_export_hex[] = "0123456789ABCDEF";

/* --- Small storage helpers --------------------------------------------- */

static bool emrtd_export_put(File* file, const char* text, size_t len) {
    if(file == NULL || len == 0) {
        return true;
    }
    return storage_file_write(file, text, len) == len;
}

static bool emrtd_export_puts(File* file, const char* text) {
    return emrtd_export_put(file, text, strlen(text));
}

/**
 * Append one formatted line.
 *
 * A write that fails makes the whole export a no-op from here on: an SD card
 * that has gone away will not come back mid read, and the read itself must
 * carry on regardless.
 */
static void emrtd_export_line(EmrtdExport* export_ctx, File* file, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

static void emrtd_export_line(EmrtdExport* export_ctx, File* file, const char* format, ...) {
    if(export_ctx->failed || file == NULL) {
        return;
    }

    char line[EMRTD_EXPORT_LINE_SIZE];
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if(written < 0) {
        return;
    }
    size_t len = (size_t)written;
    if(len >= sizeof(line)) {
        len = sizeof(line) - 1;
    }

    if(!emrtd_export_put(file, line, len) || !emrtd_export_put(file, "\n", 1)) {
        FURI_LOG_W(TAG, "Write failed, export abandoned");
        export_ctx->failed = true;
    }
}

/** An empty line; a zero length format string is a warning in this build. */
static void emrtd_export_blank(EmrtdExport* export_ctx, File* file) {
    if(export_ctx->failed || file == NULL) {
        return;
    }
    if(!emrtd_export_put(file, "\n", 1)) {
        export_ctx->failed = true;
    }
}

/** Uppercase alphanumerics only: this ends up in a path. */
static void emrtd_export_sanitise(const char* in, char* out, size_t out_size) {
    size_t n = 0;
    if(in != NULL) {
        for(size_t i = 0; in[i] != '\0' && n + 1 < out_size; i++) {
            const char c = in[i];
            if(c >= 'a' && c <= 'z') {
                out[n++] = (char)(c - 'a' + 'A');
            } else if((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                out[n++] = c;
            }
        }
    }
    if(n == 0 && out_size > 3) {
        memcpy(out, "DOC", 3);
        n = 3;
    }
    out[n] = '\0';
}

static bool emrtd_export_ensure_dir(Storage* storage, const char* path) {
    if(storage_simply_mkdir(storage, path)) {
        return true;
    }
    /* Already there is not a failure. */
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

/* --- Lifetime ----------------------------------------------------------- */

EmrtdExport* emrtd_export_alloc(const char* document_number, bool write_trace) {
    EmrtdExport* export_ctx = malloc(sizeof(EmrtdExport));
    memset(export_ctx, 0, sizeof(EmrtdExport));

    export_ctx->want_trace = write_trace;
    export_ctx->storage = furi_record_open(RECORD_STORAGE);

    char document[EMRTD_DOC_NUMBER_MAX + 1];
    emrtd_export_sanitise(document_number, document, sizeof(document));

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    snprintf(
        export_ctx->path,
        sizeof(export_ctx->path),
        EMRTD_EXPORT_DIR "/%s_%04u%02u%02u_%02u%02u",
        document,
        (unsigned)now.year,
        (unsigned)now.month,
        (unsigned)now.day,
        (unsigned)now.hour,
        (unsigned)now.minute);

    /*
     * The parent is created first because storage_simply_mkdir() only makes
     * the last component. A missing SD card fails here, which is not an error
     * worth stopping the read for - the export simply becomes a no-op.
     */
    if(!emrtd_export_ensure_dir(export_ctx->storage, EMRTD_EXPORT_DIR) ||
       !emrtd_export_ensure_dir(export_ctx->storage, export_ctx->path)) {
        FURI_LOG_W(TAG, "Cannot create %s, exporting disabled", export_ctx->path);
        export_ctx->failed = true;
        return export_ctx;
    }

    if(export_ctx->want_trace) {
        char trace_path[EMRTD_EXPORT_PATH_SIZE];
        snprintf(trace_path, sizeof(trace_path), "%s/trace.txt", export_ctx->path);

        export_ctx->trace = storage_file_alloc(export_ctx->storage);
        if(!storage_file_open(export_ctx->trace, trace_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_W(TAG, "Cannot create the trace file");
            storage_file_close(export_ctx->trace);
            storage_file_free(export_ctx->trace);
            export_ctx->trace = NULL;
        } else {
            /*
             * The first two lines are what a tool reads before anything
             * else: the version says which reader produced the file, and the
             * format number says which grammar the rest of it follows. A
             * change to any line below is a change to that number, and
             * docs/trace.md is where both are written down.
             */
            emrtd_export_trace_note(
                export_ctx, "# trace format=%u reader=%s", EMRTD_TRACE_FORMAT, EMRTD_VERSION);
            emrtd_export_trace_note(export_ctx, "# legend > command, < response, on the wire");
            emrtd_export_trace_note(
                export_ctx, "# legend >> command, << response, inside Secure Messaging");
            emrtd_export_trace_note(export_ctx, "# legend # a note: an event and its fields");
            /*
             * This file is written to be attached to a bug report, and the
             * answers of a chip that opened are the data page in the clear.
             * The warning is in the file rather than only in the handbook,
             * because a file gets attached by somebody who has not read one.
             */
            emrtd_export_trace_note(
                export_ctx,
                "# notice this file carries the document's own data in the clear;"
                " read docs/security.md before sending it anywhere");
        }
    }

    return export_ctx;
}

static void emrtd_export_close(File** file) {
    if(*file != NULL) {
        storage_file_close(*file);
        storage_file_free(*file);
        *file = NULL;
    }
}

void emrtd_export_free(EmrtdExport* export_ctx) {
    furi_check(export_ctx);

    emrtd_export_close(&export_ctx->file);
    emrtd_export_close(&export_ctx->image);
    emrtd_export_close(&export_ctx->trace);

    if(export_ctx->storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }

    /* The path is built from the document number. The allocator clears the
     * block on free (docs/platform.md, item 23); this wipe repeats that in
     * case a build does not. */
    emrtd_secure_wipe(export_ctx, sizeof(*export_ctx));
    free(export_ctx);
}

const char* emrtd_export_path(const EmrtdExport* export_ctx) {
    furi_check(export_ctx);

    return export_ctx->failed ? "" : export_ctx->path;
}

bool emrtd_export_failed(const EmrtdExport* export_ctx) {
    furi_check(export_ctx);

    return export_ctx->failed;
}

/* --- Streaming one file ------------------------------------------------- */

/** "EF.DG1" becomes "EF_DG1.bin"; the dot is not welcome in a file name. */
static void emrtd_export_file_name(const char* name, const char* suffix, char* out, size_t size) {
    size_t n = 0;
    for(size_t i = 0; name != NULL && name[i] != '\0' && n + 1 < size; i++) {
        out[n++] = name[i] == '.' ? '_' : name[i];
    }
    out[n] = '\0';

    const size_t used = strlen(out);
    if(used + strlen(suffix) + 1 <= size) {
        memcpy(out + used, suffix, strlen(suffix) + 1);
    }
}

EmrtdError emrtd_export_begin_file(EmrtdExport* export_ctx, const EmrtdFileInfo* info) {
    furi_check(export_ctx);

    emrtd_export_end_file(export_ctx);

    if(export_ctx->failed || info == NULL) {
        return EmrtdErrorNone;
    }

    char name[32];
    emrtd_export_file_name(info->name, ".bin", name, sizeof(name));

    char file_path[EMRTD_EXPORT_PATH_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/%s", export_ctx->path, name);

    export_ctx->file = storage_file_alloc(export_ctx->storage);
    if(!storage_file_open(export_ctx->file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_W(TAG, "Cannot create %s", file_path);
        emrtd_export_close(&export_ctx->file);
        export_ctx->failed = true;
        return EmrtdErrorStorage;
    }

    return EmrtdErrorNone;
}

EmrtdError emrtd_export_write(EmrtdExport* export_ctx, const uint8_t* data, size_t len) {
    furi_check(export_ctx);

    if(data == NULL) {
        return EmrtdErrorInternal;
    }
    if(export_ctx->failed || export_ctx->file == NULL || len == 0) {
        /* Still count the bytes, so that arming an image stays in step. */
        export_ctx->offset += len;
        return EmrtdErrorNone;
    }

    if(storage_file_write(export_ctx->file, data, len) != len) {
        FURI_LOG_W(TAG, "Short write, export abandoned");
        export_ctx->failed = true;
        return EmrtdErrorStorage;
    }

    /*
     * The image is a window onto the same byte stream, so only the part of
     * this chunk that falls at or after the image offset is duplicated. DG2 is
     * therefore never held twice, neither on the card nor in memory.
     */
    if(export_ctx->image != NULL && export_ctx->offset + len > export_ctx->image_offset) {
        size_t skip = 0;
        if(export_ctx->offset < export_ctx->image_offset) {
            skip = export_ctx->image_offset - export_ctx->offset;
        }
        const size_t image_len = len - skip;
        if(storage_file_write(export_ctx->image, data + skip, image_len) != image_len) {
            FURI_LOG_W(TAG, "Short write to the image file");
            export_ctx->failed = true;
            return EmrtdErrorStorage;
        }
    }

    export_ctx->offset += len;
    return EmrtdErrorNone;
}

void emrtd_export_end_file(EmrtdExport* export_ctx) {
    furi_check(export_ctx);

    emrtd_export_close(&export_ctx->file);
    emrtd_export_close(&export_ctx->image);
    export_ctx->image_armed = false;
    export_ctx->image_offset = 0;
    export_ctx->offset = 0;
}

/* --- The facial image --------------------------------------------------- */

void emrtd_export_arm_image(EmrtdExport* export_ctx, size_t offset, const char* suffix) {
    furi_check(export_ctx);

    if(export_ctx->failed || export_ctx->image_armed) {
        return;
    }
    export_ctx->image_armed = true;
    export_ctx->image_offset = offset;

    char file_path[EMRTD_EXPORT_PATH_SIZE];
    snprintf(
        file_path, sizeof(file_path), "%s/face%s", export_ctx->path, suffix ? suffix : ".bin");

    export_ctx->image = storage_file_alloc(export_ctx->storage);
    if(!storage_file_open(export_ctx->image, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_W(TAG, "Cannot create %s", file_path);
        emrtd_export_close(&export_ctx->image);
    }
}

/* --- The decoded output ------------------------------------------------- */

EmrtdError emrtd_export_write_mrz(EmrtdExport* export_ctx, const EmrtdMrz* mrz) {
    furi_check(export_ctx);

    if(export_ctx->failed || mrz == NULL) {
        return EmrtdErrorNone;
    }

    char file_path[EMRTD_EXPORT_PATH_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/mrz.txt", export_ctx->path);

    File* file = storage_file_alloc(export_ctx->storage);
    if(!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(file);
        storage_file_free(file);
        export_ctx->failed = true;
        return EmrtdErrorStorage;
    }

    char name[88];
    emrtd_mrz_full_name(mrz, name, sizeof(name));
    char birth[16];
    emrtd_mrz_format_date(mrz->date_of_birth, false, birth, sizeof(birth));
    char expiry[16];
    emrtd_mrz_format_date(mrz->date_of_expiry, true, expiry, sizeof(expiry));

    emrtd_export_line(export_ctx, file, "Name:            %s", name);
    emrtd_export_line(export_ctx, file, "Document type:   %s", mrz->document_type);
    emrtd_export_line(export_ctx, file, "Document number: %s", mrz->document_number);
    emrtd_export_line(export_ctx, file, "Issuing state:   %s", mrz->issuing_state);
    emrtd_export_line(export_ctx, file, "Nationality:     %s", mrz->nationality);
    emrtd_export_line(export_ctx, file, "Date of birth:   %s", birth);
    emrtd_export_line(export_ctx, file, "Sex:             %s", mrz->sex);
    emrtd_export_line(export_ctx, file, "Date of expiry:  %s", expiry);
    if(mrz->optional_data[0] != '\0') {
        emrtd_export_line(export_ctx, file, "Optional data:   %s", mrz->optional_data);
    }
    emrtd_export_line(
        export_ctx, file, "Check digits:    %s", mrz->check_digits_valid ? "valid" : "INVALID");

    storage_file_close(file);
    storage_file_free(file);
    return export_ctx->failed ? EmrtdErrorStorage : EmrtdErrorNone;
}

static const char* emrtd_export_file_state_text(const EmrtdFileResult* file) {
    switch(file->state) {
    case EmrtdFileStateAbsent:
        return "not on the chip";
    case EmrtdFileStatePending:
    case EmrtdFileStateReading:
        return "not reached";
    case EmrtdFileStateRead:
        return "read";
    case EmrtdFileStateSkipped:
        return "skipped";
    case EmrtdFileStateFailed:
        return "FAILED";
    default:
        return "unknown";
    }
}

static const char* emrtd_export_hash_state_text(EmrtdHashState state) {
    switch(state) {
    case EmrtdHashStateMatch:
        return "hash match";
    case EmrtdHashStateMismatch:
        return "HASH MISMATCH";
    case EmrtdHashStateNotListed:
        return "not listed in EF.SOD";
    case EmrtdHashStateUnsupportedDigest:
        return "digest not supported";
    default:
        return "not checked";
    }
}

static void emrtd_export_write_access(
    EmrtdExport* export_ctx,
    File* file,
    const EmrtdReadResult* result,
    const EmrtdWorkerConfig* config) {
    emrtd_export_line(export_ctx, file, "--- Access control ---");
    emrtd_export_line(
        export_ctx, file, "Requested:        %s", emrtd_access_method_name(config->method));
    /*
     * Whether EF.CardAccess could be read is the single fact that decides
     * between PACE and BAC, so it belongs here even though the file itself is
     * not part of the application's catalogue.
     */
    emrtd_export_line(
        export_ctx,
        file,
        "EF.CardAccess:    %s",
        result->card_access_read ? "read" : "not available");

    if(!result->authenticated) {
        emrtd_export_line(export_ctx, file, "Result:           not authenticated");
        emrtd_export_blank(export_ctx, file);
        return;
    }

    emrtd_export_line(export_ctx, file, "Protocol:         %s", result->access.protocol);
    emrtd_export_line(export_ctx, file, "Details:          %s", result->access.summary);

    /*
     * The PACE object identifier and the curve are what a failure report needs
     * most: they say exactly which variant of the protocol the chip asked for.
     */
    const EmrtdPaceInfo* pace = &result->security_infos.pace;
    if(result->security_infos.has_pace) {
        if(pace->oid_name != NULL) {
            emrtd_export_line(export_ctx, file, "PACE protocol:    %s", pace->oid_name);
        }
        if(pace->oid_len > 0) {
            char oid[2 * EMRTD_OID_MAX + 1];
            size_t n = 0;
            for(size_t i = 0; i < pace->oid_len && n + 2 < sizeof(oid); i++) {
                oid[n++] = emrtd_export_hex[pace->oid[i] >> 4];
                oid[n++] = emrtd_export_hex[pace->oid[i] & 0x0F];
            }
            oid[n] = '\0';
            emrtd_export_line(export_ctx, file, "PACE OID (DER):   %s", oid);
        }
        emrtd_export_line(
            export_ctx, file, "PACE cipher:      %s", emrtd_cipher_name(pace->cipher));
        if(pace->curve != NULL) {
            emrtd_export_line(
                export_ctx,
                file,
                "PACE curve:       %s (%u bit)",
                pace->curve->name,
                (unsigned)pace->curve->bits);
        } else {
            emrtd_export_line(
                export_ctx,
                file,
                "PACE curve:       parameter id %u",
                (unsigned)pace->parameter_id);
        }
    }
    emrtd_export_blank(export_ctx, file);
}

static void
    emrtd_export_write_files(EmrtdExport* export_ctx, File* file, const EmrtdReadResult* result) {
    emrtd_export_line(export_ctx, file, "--- Files ---");

    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        const EmrtdFileResult* entry = &result->files[i];
        if(entry->state == EmrtdFileStateAbsent) {
            continue;
        }

        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)i);
        const char* name = info != NULL ? info->name : "EF.?";

        if(entry->state == EmrtdFileStateRead) {
            emrtd_export_line(
                export_ctx,
                file,
                "%-8s %6u bytes  %s",
                name,
                (unsigned)entry->size,
                emrtd_export_hash_state_text(entry->hash_state));
        } else if(entry->error != EmrtdErrorNone) {
            emrtd_export_line(
                export_ctx,
                file,
                "%-8s %s: %s",
                name,
                emrtd_export_file_state_text(entry),
                emrtd_error_text(entry->error));
        } else {
            emrtd_export_line(
                export_ctx, file, "%-8s %s", name, emrtd_export_file_state_text(entry));
        }
    }
    emrtd_export_blank(export_ctx, file);
}

EmrtdError emrtd_export_write_report(
    EmrtdExport* export_ctx,
    const EmrtdReadResult* result,
    const EmrtdWorkerConfig* config) {
    furi_check(export_ctx);

    if(result == NULL || config == NULL) {
        return EmrtdErrorInternal;
    }
    if(export_ctx->failed) {
        return EmrtdErrorNone;
    }

    char file_path[EMRTD_EXPORT_PATH_SIZE];
    snprintf(file_path, sizeof(file_path), "%s/report.txt", export_ctx->path);

    File* file = storage_file_alloc(export_ctx->storage);
    if(!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(file);
        storage_file_free(file);
        export_ctx->failed = true;
        return EmrtdErrorStorage;
    }

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    emrtd_export_line(export_ctx, file, "============================================");
    emrtd_export_line(export_ctx, file, "ELECTRONIC TRAVEL DOCUMENT READ REPORT");
    emrtd_export_line(export_ctx, file, "============================================");
    emrtd_export_blank(export_ctx, file);
    emrtd_export_line(
        export_ctx, file, "Reader:           eMRTD " EMRTD_VERSION " (Flipper Zero)");
    emrtd_export_line(
        export_ctx,
        file,
        "Read at:          %04u-%02u-%02u %02u:%02u:%02u",
        (unsigned)now.year,
        (unsigned)now.month,
        (unsigned)now.day,
        (unsigned)now.hour,
        (unsigned)now.minute,
        (unsigned)now.second);
    emrtd_export_blank(export_ctx, file);

    emrtd_export_write_access(export_ctx, file, result, config);

    if(result->has_com) {
        emrtd_export_line(export_ctx, file, "--- EF.COM ---");
        emrtd_export_line(export_ctx, file, "LDS version:      %s", result->com.lds_version);
        emrtd_export_line(export_ctx, file, "Unicode version:  %s", result->com.unicode_version);
        emrtd_export_line(
            export_ctx, file, "Announced groups: %u", (unsigned)result->com.tag_count);
        emrtd_export_blank(export_ctx, file);
    }

    emrtd_export_write_files(export_ctx, file, result);

    if(result->has_mrz) {
        char name[88];
        emrtd_mrz_full_name(&result->mrz, name, sizeof(name));
        char birth[16];
        emrtd_mrz_format_date(result->mrz.date_of_birth, false, birth, sizeof(birth));
        char expiry[16];
        emrtd_mrz_format_date(result->mrz.date_of_expiry, true, expiry, sizeof(expiry));

        emrtd_export_line(export_ctx, file, "--- MRZ (DG1) ---");
        emrtd_export_line(export_ctx, file, "Name:             %s", name);
        emrtd_export_line(export_ctx, file, "Document number:  %s", result->mrz.document_number);
        emrtd_export_line(export_ctx, file, "Nationality:      %s", result->mrz.nationality);
        emrtd_export_line(export_ctx, file, "Date of birth:    %s", birth);
        emrtd_export_line(export_ctx, file, "Sex:              %s", result->mrz.sex);
        emrtd_export_line(export_ctx, file, "Date of expiry:   %s", expiry);
        emrtd_export_blank(export_ctx, file);
    }

    if(result->has_dg11 && result->dg11.any) {
        emrtd_export_line(export_ctx, file, "--- Additional personal details (DG11) ---");
        if(result->dg11.full_name[0] != '\0') {
            emrtd_export_line(export_ctx, file, "Full name:        %s", result->dg11.full_name);
        }
        if(result->dg11.personal_number[0] != '\0') {
            emrtd_export_line(
                export_ctx, file, "Personal number:  %s", result->dg11.personal_number);
        }
        if(result->dg11.place_of_birth[0] != '\0') {
            emrtd_export_line(
                export_ctx, file, "Place of birth:   %s", result->dg11.place_of_birth);
        }
        if(result->dg11.address[0] != '\0') {
            emrtd_export_line(export_ctx, file, "Address:          %s", result->dg11.address);
        }
        if(result->dg11.telephone[0] != '\0') {
            emrtd_export_line(export_ctx, file, "Telephone:        %s", result->dg11.telephone);
        }
        if(result->dg11.profession[0] != '\0') {
            emrtd_export_line(export_ctx, file, "Profession:       %s", result->dg11.profession);
        }
        emrtd_export_blank(export_ctx, file);
    }

    if(result->has_dg12 && result->dg12.any) {
        emrtd_export_line(export_ctx, file, "--- Document details (DG12) ---");
        if(result->dg12.issuing_authority[0] != '\0') {
            emrtd_export_line(
                export_ctx, file, "Issuing authority: %s", result->dg12.issuing_authority);
        }
        if(result->dg12.date_of_issue[0] != '\0') {
            emrtd_export_line(
                export_ctx, file, "Date of issue:     %s", result->dg12.date_of_issue);
        }
        if(result->dg12.endorsements[0] != '\0') {
            emrtd_export_line(
                export_ctx, file, "Endorsements:      %s", result->dg12.endorsements);
        }
        emrtd_export_blank(export_ctx, file);
    }

    if(result->has_dg15) {
        emrtd_export_line(export_ctx, file, "--- Active Authentication key (DG15) ---");
        emrtd_export_line(export_ctx, file, "Algorithm:        %s", result->dg15.algorithm);
        emrtd_export_line(
            export_ctx, file, "Key size:         %u bit", (unsigned)result->dg15.key_bits);
        emrtd_export_blank(export_ctx, file);
    }

    emrtd_export_line(export_ctx, file, "--- Passive authentication ---");
    if(result->has_sod) {
        emrtd_export_line(export_ctx, file, "Digest:           %s", result->sod.digest_algorithm);
        emrtd_export_line(
            export_ctx, file, "Hashes listed:    %u", (unsigned)result->sod.hash_count);
        emrtd_export_line(
            export_ctx,
            file,
            "Hashes verified:  %u of %u matched",
            (unsigned)result->hashes_matched,
            (unsigned)result->hashes_checked);
        emrtd_export_line(
            export_ctx,
            file,
            "Signer algorithm: %s",
            result->sod.signer_algorithm[0] != '\0' ? result->sod.signer_algorithm : "unknown");
        /*
         * ICAO 9303-11 part 4: a complete passive authentication also checks
         * the Document Signer's signature against a country signing
         * certificate. That needs RSA and a certificate store, neither of
         * which this build has - see docs/platform.md.
         */
        emrtd_export_line(export_ctx, file, "EF.SOD signature: not verified on this device");
    } else {
        emrtd_export_line(export_ctx, file, "EF.SOD was not read; nothing could be verified.");
    }
    emrtd_export_blank(export_ctx, file);

    if(result->error != EmrtdErrorNone) {
        emrtd_export_line(export_ctx, file, "--- How the read ended ---");
        emrtd_export_line(export_ctx, file, "%s", emrtd_error_text(result->error));
        const char* stopped = emrtd_worker_stopped_text(result);
        if(stopped != NULL) {
            emrtd_export_line(export_ctx, file, "%s", stopped);
        }
        emrtd_export_line(export_ctx, file, "%s", emrtd_error_hint(result->error));
        emrtd_export_blank(export_ctx, file);
    }

    storage_file_close(file);
    storage_file_free(file);
    return export_ctx->failed ? EmrtdErrorStorage : EmrtdErrorNone;
}

/* --- The diagnostic trace ----------------------------------------------- */

/**
 * Hex encode @p len bytes onto the open trace.
 *
 * Encoded in slices so that a forty kilobyte data group does not need a
 * matching buffer; this runs on the NFC thread, whose stack is small.
 */
static bool emrtd_export_trace_hex(EmrtdExport* export_ctx, const uint8_t* data, size_t len) {
    char slice[2 * EMRTD_EXPORT_TRACE_SLICE];
    size_t done = 0;

    while(done < len && data != NULL) {
        size_t n = len - done;
        if(n > EMRTD_EXPORT_TRACE_SLICE) {
            n = EMRTD_EXPORT_TRACE_SLICE;
        }
        for(size_t i = 0; i < n; i++) {
            slice[2 * i] = emrtd_export_hex[data[done + i] >> 4];
            slice[2 * i + 1] = emrtd_export_hex[data[done + i] & 0x0F];
        }
        if(!emrtd_export_put(export_ctx->trace, slice, 2 * n)) {
            return false;
        }
        done += n;
    }

    return true;
}

void emrtd_export_trace(
    EmrtdExport* export_ctx,
    const char* label,
    const uint8_t* data,
    size_t len) {
    furi_check(export_ctx);

    if(export_ctx->trace == NULL || label == NULL) {
        return;
    }

    if(!emrtd_export_puts(export_ctx->trace, label)) {
        return;
    }
    if(!emrtd_export_trace_hex(export_ctx, data, len)) {
        return;
    }

    emrtd_export_put(export_ctx->trace, "\n", 1);
}

void emrtd_export_trace_response(
    EmrtdExport* export_ctx,
    const char* label,
    const uint8_t* data,
    size_t len,
    uint16_t sw) {
    furi_check(export_ctx);

    if(export_ctx->trace == NULL || label == NULL) {
        return;
    }

    const uint8_t status[2] = {(uint8_t)(sw >> 8), (uint8_t)(sw & 0xFF)};

    if(!emrtd_export_puts(export_ctx->trace, label)) {
        return;
    }
    if(!emrtd_export_trace_hex(export_ctx, data, len)) {
        return;
    }
    if(!emrtd_export_trace_hex(export_ctx, status, sizeof(status))) {
        return;
    }

    emrtd_export_put(export_ctx->trace, "\n", 1);
}

void emrtd_export_trace_note(EmrtdExport* export_ctx, const char* format, ...) {
    furi_check(export_ctx);

    if(export_ctx->trace == NULL || format == NULL) {
        return;
    }

    /*
     * A note is built here rather than at the call site so that the fifty odd
     * places that write one do not each keep a buffer on the NFC thread's
     * stack. A line longer than this is truncated rather than split: a note
     * is a comment on the hex around it, and losing the tail of one costs
     * nothing that the exchange itself does not already say.
     */
    char line[EMRTD_EXPORT_NOTE_SIZE];
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if(written <= 0) {
        return;
    }
    size_t len = (size_t)written;
    if(len >= sizeof(line)) {
        len = sizeof(line) - 1;
    }

    if(!emrtd_export_put(export_ctx->trace, line, len)) {
        return;
    }
    emrtd_export_put(export_ctx->trace, "\n", 1);
}
