/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Remembering the credentials and the options between runs.
 *
 * The file this writes holds the values that open the chip - the three from the
 * machine readable zone, or a card access number - which is to say the key to
 * someone's identity document. It is stored in the clear, because the Flipper
 * has nowhere to keep a secret that the person holding it could not read
 * anyway, so the file says what it is in its header and the user can delete it
 * from the Document scene at any time.
 *
 * Nothing read back from the file is trusted: a hand edited or truncated file
 * must leave the application in the same state as no file at all.
 */
#include "emrtd_i.h"

#include <lib/flipper_format/flipper_format.h>

#include "access/emrtd_access.h"
#include "emrtd_wipe.h"

#define TAG "EmrtdSettings"

#define EMRTD_SETTINGS_HEADER  "eMRTD reader settings"
/*
 * Bumped whenever the meaning of the file changes, not when the reader's
 * version does: a patch release must not cost the user their stored details.
 * A file written by any other version is discarded *and deleted*, because it
 * may hold credentials under keys this build no longer reads - leaving it in
 * place would leave them on the card with nothing able to clear them.
 *
 * 1 -> 2: added EMRTD_KEY_WIPE, and the credential defaults became opt-in.
 * 2 -> 3: removed EMRTD_KEY_WIPE; the credentials are kept on SD by default.
 */
#define EMRTD_SETTINGS_VERSION (3)

#define EMRTD_KEY_DOC_NUMBER "Document Number"
#define EMRTD_KEY_BIRTH      "Date of Birth"
#define EMRTD_KEY_EXPIRY     "Date of Expiry"
#define EMRTD_KEY_CAN        "Card Access Number"
#define EMRTD_KEY_REMEMBER   "Remember Credentials"
#define EMRTD_KEY_METHOD     "Access Method"
#define EMRTD_KEY_FILES      "Data Groups"
#define EMRTD_KEY_EXPORT     "Export To SD"
#define EMRTD_KEY_TRACE      "Write Trace"
/* Informational only: never compared, so a patch release changes nothing. */
#define EMRTD_KEY_READER     "Reader Version"

/** The MRZ alphabet of ICAO 9303-3, section 4.2.2. */
static bool emrtd_settings_is_mrz_string(const char* text) {
    for(const char* c = text; *c != '\0'; c++) {
        bool ok = (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || (*c == '<');
        if(!ok) {
            return false;
        }
    }
    return true;
}

static bool emrtd_settings_is_digits(const char* text, size_t expected_len) {
    if(strlen(text) != expected_len) {
        return false;
    }
    for(size_t i = 0; i < expected_len; i++) {
        if(text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    return true;
}

/**
 * Read one string key into a fixed field.
 *
 * Every key is optional and the file may list them in any order, so the
 * format is rewound before each lookup. A value that does not fit the field
 * or is not of the expected shape is dropped rather than truncated: half a
 * document number would fail authentication with no hint as to why.
 */
static bool emrtd_settings_read_field(
    FlipperFormat* file,
    FuriString* scratch,
    const char* key,
    char* out,
    size_t out_size) {
    if(!flipper_format_rewind(file)) {
        return false;
    }
    if(!flipper_format_read_string(file, key, scratch)) {
        return false;
    }

    const char* value = furi_string_get_cstr(scratch);
    if(furi_string_size(scratch) >= out_size) {
        FURI_LOG_W(TAG, "%s does not fit, ignored", key);
        return false;
    }

    strlcpy(out, value, out_size);
    return true;
}

static bool emrtd_settings_read_bool(FlipperFormat* file, const char* key, bool* out) {
    if(!flipper_format_rewind(file)) {
        return false;
    }
    return flipper_format_read_bool(file, key, out, 1);
}

static bool emrtd_settings_read_uint32(FlipperFormat* file, const char* key, uint32_t* out) {
    if(!flipper_format_rewind(file)) {
        return false;
    }
    return flipper_format_read_uint32(file, key, out, 1);
}

bool emrtd_settings_load(Emrtd* app) {
    furi_assert(app);

    FlipperFormat* file = flipper_format_file_alloc(app->storage);
    FuriString* scratch = furi_string_alloc();
    EmrtdCredentials* credentials = &app->config.credentials;
    bool loaded = false;
    bool stale = false;
    /* Hoisted so that the wipe below it is in scope; it carries the document
     * number and the CAN on their way out of the file. */
    char buffer[EMRTD_DOC_NUMBER_MAX + 1];

    do {
        if(!flipper_format_file_open_existing(file, EMRTD_SETTINGS_PATH)) {
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(file, scratch, &version)) {
            break;
        }
        if(!furi_string_equal_str(scratch, EMRTD_SETTINGS_HEADER) ||
           version != EMRTD_SETTINGS_VERSION) {
            FURI_LOG_W(
                TAG,
                "Settings file is version %lu, not %u; discarding it",
                (unsigned long)version,
                EMRTD_SETTINGS_VERSION);
            stale = true;
            break;
        }

        if(emrtd_settings_read_field(file, scratch, EMRTD_KEY_DOC_NUMBER, buffer, sizeof(buffer)) &&
           emrtd_settings_is_mrz_string(buffer)) {
            strlcpy(credentials->document_number, buffer, sizeof(credentials->document_number));
        }

        if(emrtd_settings_read_field(file, scratch, EMRTD_KEY_BIRTH, buffer, sizeof(buffer)) &&
           emrtd_settings_is_digits(buffer, EMRTD_DATE_LEN)) {
            strlcpy(credentials->date_of_birth, buffer, sizeof(credentials->date_of_birth));
        }

        if(emrtd_settings_read_field(file, scratch, EMRTD_KEY_EXPIRY, buffer, sizeof(buffer)) &&
           emrtd_settings_is_digits(buffer, EMRTD_DATE_LEN)) {
            strlcpy(credentials->date_of_expiry, buffer, sizeof(credentials->date_of_expiry));
        }

        if(emrtd_settings_read_field(file, scratch, EMRTD_KEY_CAN, buffer, sizeof(buffer)) &&
           emrtd_settings_is_digits(buffer, EMRTD_CAN_MAX)) {
            strlcpy(credentials->can, buffer, sizeof(credentials->can));
            credentials->has_can = true;
        }

        emrtd_settings_read_bool(file, EMRTD_KEY_REMEMBER, &app->remember_credentials);
        emrtd_settings_read_bool(file, EMRTD_KEY_EXPORT, &app->config.export_to_sd);
        emrtd_settings_read_bool(file, EMRTD_KEY_TRACE, &app->config.write_trace);

        uint32_t number = 0;
        if(emrtd_settings_read_uint32(file, EMRTD_KEY_METHOD, &number) &&
           number <= EmrtdAccessMethodBac) {
            app->config.method = (EmrtdAccessMethod)number;
        }

        if(emrtd_settings_read_uint32(file, EMRTD_KEY_FILES, &number)) {
            /*
             * Only bits that name a file the reader is willing to ask for are
             * kept. The default mask is exactly that set, so it drops both the
             * bits that stand for nothing and the two groups behind Extended
             * Access Control: a file that turned DG3 or DG4 on would be obeyed
             * on every later run, the Data groups screen shows those rows as
             * "EAC" with no way to clear them, and the screen would write the
             * mask back out on the way out. EF.COM and EF.SOD are not
             * optional; the reader needs both.
             */
            app->config.files = ((EmrtdFileMask)number & emrtd_file_default_mask()) |
                                EMRTD_FILE_BIT(EmrtdFileCom) | EMRTD_FILE_BIT(EmrtdFileSod);
        }

        loaded = true;
    } while(false);

    /* Both held the document number and the CAN on their way to the struct. */
    emrtd_secure_wipe(buffer, sizeof(buffer));
    furi_string_free(scratch);
    flipper_format_free(file);

    /*
     * A file this build cannot read is removed rather than left behind: it was
     * written by a version whose credential keys may differ, and Forget would
     * not know to clear them.
     */
    if(stale && storage_file_exists(app->storage, EMRTD_SETTINGS_PATH)) {
        storage_simply_remove(app->storage, EMRTD_SETTINGS_PATH);
    }

    return loaded;
}

bool emrtd_settings_save(Emrtd* app) {
    furi_assert(app);

    /*
     * The settings live beside the exports, in a directory the application is
     * free to create; nothing else does it for us, because the path is
     * spelled out rather than resolved through APP_DATA_PATH.
     */
    if(!storage_dir_exists(app->storage, EMRTD_EXPORT_DIR)) {
        storage_simply_mkdir(app->storage, EMRTD_EXPORT_DIR);
    }

    FlipperFormat* file = flipper_format_file_alloc(app->storage);
    const EmrtdCredentials* credentials = &app->config.credentials;
    bool saved = false;

    do {
        if(!flipper_format_file_open_always(file, EMRTD_SETTINGS_PATH)) {
            FURI_LOG_E(TAG, "Cannot write " EMRTD_SETTINGS_PATH);
            break;
        }
        if(!flipper_format_write_header_cstr(file, EMRTD_SETTINGS_HEADER, EMRTD_SETTINGS_VERSION)) {
            break;
        }
        if(!flipper_format_write_comment_cstr(
               file, "These values unlock an electronic travel document. Keep or delete them")) {
            break;
        }
        if(!flipper_format_write_comment_cstr(
               file, "as you would the document itself: Document - Forget stored data.")) {
            break;
        }

        /*
         * Only values that exist are written. Forget clears the credentials in
         * memory and then saves, to put the options back; without this an
         * empty document number would be written straight back into the file
         * Forget had just emptied, and the user would be looking at a settings
         * file that still names the keys it was told had gone.
         */
        if(app->remember_credentials) {
            if(credentials->document_number[0] != '\0' &&
               !flipper_format_write_string_cstr(
                   file, EMRTD_KEY_DOC_NUMBER, credentials->document_number)) {
                break;
            }
            if(credentials->date_of_birth[0] != '\0' &&
               !flipper_format_write_string_cstr(
                   file, EMRTD_KEY_BIRTH, credentials->date_of_birth)) {
                break;
            }
            if(credentials->date_of_expiry[0] != '\0' &&
               !flipper_format_write_string_cstr(
                   file, EMRTD_KEY_EXPIRY, credentials->date_of_expiry)) {
                break;
            }
            if(credentials->has_can && credentials->can[0] != '\0') {
                if(!flipper_format_write_string_cstr(file, EMRTD_KEY_CAN, credentials->can)) {
                    break;
                }
            }
        }

        if(!flipper_format_write_bool(file, EMRTD_KEY_REMEMBER, &app->remember_credentials, 1)) {
            break;
        }

        const uint32_t method = (uint32_t)app->config.method;
        if(!flipper_format_write_uint32(file, EMRTD_KEY_METHOD, &method, 1)) {
            break;
        }

        const uint32_t files = (uint32_t)app->config.files;
        if(!flipper_format_write_uint32(file, EMRTD_KEY_FILES, &files, 1)) {
            break;
        }

        if(!flipper_format_write_bool(file, EMRTD_KEY_EXPORT, &app->config.export_to_sd, 1)) {
            break;
        }
        if(!flipper_format_write_bool(file, EMRTD_KEY_TRACE, &app->config.write_trace, 1)) {
            break;
        }
        if(!flipper_format_write_string_cstr(file, EMRTD_KEY_READER, EMRTD_VERSION)) {
            break;
        }

        saved = true;
    } while(false);

    flipper_format_free(file);

    return saved;
}

bool emrtd_settings_delete(Emrtd* app) {
    furi_assert(app);

    /*
     * Forgetting has to be true in both places: the file on the card and the
     * copy in memory. The options are deliberately kept, because they say
     * nothing about the holder.
     */
    bool removed = true;
    if(storage_file_exists(app->storage, EMRTD_SETTINGS_PATH)) {
        removed = storage_simply_remove(app->storage, EMRTD_SETTINGS_PATH);
    }

    emrtd_secure_wipe(&app->config.credentials, sizeof(app->config.credentials));

    return removed;
}
