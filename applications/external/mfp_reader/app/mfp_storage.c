#include "mfp_storage.h"
#include "mfp_app.h"
#include "mfp_keys.h"

#include <storage/storage.h>
#include <furi.h>
#include <string.h>
#include <stdlib.h>

/* ---- Helper: build filename from UID ---- */

static void mfp_storage_build_path(MfpApp* app) {
    storage_simply_mkdir(app->storage, MFP_APP_FOLDER);

    FuriString* path = furi_string_alloc_printf(MFP_APP_FOLDER "/MFP");
    for(uint8_t i = 0; i < app->version.uid_len; i++)
        furi_string_cat_printf(path, "%02X", app->version.uid[i]);
    furi_string_cat_str(path, MFP_FILE_EXT);

    strlcpy(app->save_path, furi_string_get_cstr(path), sizeof(app->save_path));
    furi_string_free(path);
}

/* ---- Save (single sector, Version 1) ---- */

bool mfp_storage_save(MfpApp* app) {
    mfp_storage_build_path(app);

    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, app->save_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) {
        storage_file_free(f);
        return false;
    }

    FuriString* line = furi_string_alloc();

#define WRITE_LINE(s) \
    storage_file_write(f, furi_string_get_cstr(s), furi_string_size(s))

    furi_string_set_str(line, "Filetype: MFP Reader\nVersion: 1\n");
    WRITE_LINE(line);

    furi_string_set_str(line, "UID:");
    for(uint8_t i = 0; i < app->version.uid_len; i++)
        furi_string_cat_printf(line, " %02X", app->version.uid[i]);
    furi_string_cat_str(line, "\n");
    WRITE_LINE(line);

    furi_string_printf(line, "Security Level: %d\n", (int)app->version.sl);
    WRITE_LINE(line);

    furi_string_printf(
        line, "Card Size: %s\n", app->version.size == MfpSize4K ? "4K" : "2K");
    WRITE_LINE(line);

    furi_string_printf(line, "Sector: %d\n", (int)app->target_sector);
    WRITE_LINE(line);

    furi_string_printf(
        line, "Allow Overwrite: %s\n", app->allow_overwrite ? "yes" : "no");
    WRITE_LINE(line);

    uint8_t first_block = mfp_sector_first_block(app->version.size, app->target_sector);
    for(uint8_t b = 0; b < app->blocks_read; b++) {
        furi_string_printf(line, "Block %d:", first_block + b);
        for(uint8_t j = 0; j < MFP_BLOCK_SIZE; j++)
            furi_string_cat_printf(line, " %02X", app->blocks[b][j]);
        furi_string_cat_str(line, "\n");
        WRITE_LINE(line);
    }

#undef WRITE_LINE

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);
    return true;
}

/* ---- Save All (multi-sector, Version 2) ---- */

static bool mfp_storage_write_all_to_path(MfpApp* app, const char* path) {
    storage_simply_mkdir(app->storage, MFP_APP_FOLDER);

    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) {
        storage_file_free(f);
        return false;
    }

    FuriString* line = furi_string_alloc();

#define WRITE_LINE(s) \
    storage_file_write(f, furi_string_get_cstr(s), furi_string_size(s))

    furi_string_set_str(line, "Filetype: MFP Reader\nVersion: 2\n");
    WRITE_LINE(line);

    furi_string_set_str(line, "UID:");
    for(uint8_t i = 0; i < app->version.uid_len; i++)
        furi_string_cat_printf(line, " %02X", app->version.uid[i]);
    furi_string_cat_str(line, "\n");
    WRITE_LINE(line);

    furi_string_printf(line, "Security Level: %d\n", (int)app->version.sl);
    WRITE_LINE(line);

    furi_string_printf(
        line, "Card Size: %s\n", app->version.size == MfpSize4K ? "4K" : "2K");
    WRITE_LINE(line);

    furi_string_printf(line, "Sectors Read: %d\n", (int)app->scan_sectors_ok);
    WRITE_LINE(line);

    furi_string_printf(
        line, "Allow Overwrite: %s\n", app->allow_overwrite ? "yes" : "no");
    WRITE_LINE(line);

    uint8_t total = mfp_sector_count(app->version.size);
    for(uint8_t s = 0; s < total; s++) {
        const MfpSectorResult* r = &app->sector_results[s];

        if(r->status == MfpSectorOk) {
            /* Format: "Sector N: OK KeyA <32hex> KeyB <32hex>"
             * If only one key is known, emit just that token. */
            furi_string_printf(line, "Sector %d: OK", (int)s);
            if(r->key_a_found) {
                furi_string_cat_str(line, " KeyA ");
                for(uint8_t k = 0; k < MFP_AES_KEY_SIZE; k++)
                    furi_string_cat_printf(line, "%02X", r->key_a[k]);
            }
            if(r->key_b_found) {
                furi_string_cat_str(line, " KeyB ");
                for(uint8_t k = 0; k < MFP_AES_KEY_SIZE; k++)
                    furi_string_cat_printf(line, "%02X", r->key_b[k]);
            }
            furi_string_cat_str(line, "\n");
            WRITE_LINE(line);

            uint8_t fb = mfp_sector_first_block(app->version.size, s);
            for(uint8_t b = 0; b < r->blocks_read; b++) {
                furi_string_printf(line, "Block %d:", (int)(fb + b));
                for(uint8_t j = 0; j < MFP_BLOCK_SIZE; j++)
                    furi_string_cat_printf(line, " %02X", app->blocks[fb + b][j]);
                furi_string_cat_str(line, "\n");
                WRITE_LINE(line);
            }
        } else if(r->status == MfpSectorAuthFail) {
            furi_string_printf(line, "Sector %d: AuthFail\n", (int)s);
            WRITE_LINE(line);
        } else if(r->status == MfpSectorReadFail) {
            furi_string_printf(line, "Sector %d: ReadFail\n", (int)s);
            WRITE_LINE(line);
        }
        /* MfpSectorNone — skip */
    }

#undef WRITE_LINE

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);
    return true;
}

bool mfp_storage_save_all(MfpApp* app) {
    mfp_storage_build_path(app);
    return mfp_storage_write_all_to_path(app, app->save_path);
}

bool mfp_storage_save_all_to_path(MfpApp* app, const char* path) {
    if(!path || !*path) return false;
    strlcpy(app->save_path, path, sizeof(app->save_path));
    return mfp_storage_write_all_to_path(app, app->save_path);
}

bool mfp_storage_save_modifications(MfpApp* app, char* out_path, size_t out_path_size) {
    /* Determine target path:
     *  - loaded from a file: write back to that exact file
     *  - fresh scan: build the standard UID-based path */
    if(!app->loaded_from_file || app->save_path[0] == '\0') {
        mfp_storage_build_path(app);
    }

    bool ok = mfp_storage_write_all_to_path(app, app->save_path);
    if(ok && out_path && out_path_size > 0) {
        strlcpy(out_path, app->save_path, out_path_size);
    }
    return ok;
}

/* ---- Load ---- */

static void mfp_storage_parse_v1(MfpApp* app, char* buf) {
    char* p = buf;
    while(p && *p) {
        char* end = strchr(p, '\n');
        if(end) *end = '\0';

        if(strncmp(p, "UID:", 4) == 0) {
            const char* tok = p + 4;
            uint8_t len = 0;
            while(*tok && len < 7) {
                while(*tok == ' ') tok++;
                if(!*tok) break;
                app->version.uid[len++] = (uint8_t)strtoul(tok, (char**)&tok, 16);
            }
            app->version.uid_len = len;
        } else if(strncmp(p, "Security Level: ", 16) == 0) {
            app->version.sl = (MfpSecurityLevel)atoi(p + 16);
        } else if(strncmp(p, "Card Size: ", 11) == 0) {
            app->version.size = (strcmp(p + 11, "4K") == 0) ? MfpSize4K : MfpSize2K;
        } else if(strncmp(p, "Sector: ", 8) == 0) {
            app->target_sector = (uint8_t)atoi(p + 8);
        } else if(strncmp(p, "Allow Overwrite: ", 17) == 0) {
            app->allow_overwrite = (strncmp(p + 17, "yes", 3) == 0);
        } else if(strncmp(p, "Block ", 6) == 0 && app->blocks_read < UINT8_MAX) {
            const char* col = strchr(p, ':');
            if(col) {
                col++;
                uint8_t j = 0;
                while(*col && j < MFP_BLOCK_SIZE) {
                    while(*col == ' ') col++;
                    if(!*col) break;
                    app->blocks[app->blocks_read][j++] =
                        (uint8_t)strtoul(col, (char**)&col, 16);
                }
                app->blocks_read++;
            }
        }

        p = end ? end + 1 : NULL;
    }
}

static void mfp_storage_parse_v2(MfpApp* app, char* buf) {
    uint8_t cur_sector = 0;
    bool in_sector = false;

    char* p = buf;
    while(p && *p) {
        char* end = strchr(p, '\n');
        if(end) *end = '\0';

        if(strncmp(p, "UID:", 4) == 0) {
            const char* tok = p + 4;
            uint8_t len = 0;
            while(*tok && len < 7) {
                while(*tok == ' ') tok++;
                if(!*tok) break;
                app->version.uid[len++] = (uint8_t)strtoul(tok, (char**)&tok, 16);
            }
            app->version.uid_len = len;
        } else if(strncmp(p, "Security Level: ", 16) == 0) {
            app->version.sl = (MfpSecurityLevel)atoi(p + 16);
        } else if(strncmp(p, "Card Size: ", 11) == 0) {
            app->version.size = (strcmp(p + 11, "4K") == 0) ? MfpSize4K : MfpSize2K;
        } else if(strncmp(p, "Sectors Read: ", 14) == 0) {
            app->scan_sectors_ok = (uint8_t)atoi(p + 14);
        } else if(strncmp(p, "Allow Overwrite: ", 17) == 0) {
            app->allow_overwrite = (strncmp(p + 17, "yes", 3) == 0);
        } else if(strncmp(p, "Sector ", 7) == 0) {
            /* Parse "Sector N: OK KeyA HEXHEX..." or "Sector N: AuthFail" */
            const char* sp = p + 7;
            cur_sector = (uint8_t)strtoul(sp, (char**)&sp, 10);
            if(cur_sector < MFP_SECTORS_4K) {
                /* skip ": " */
                while(*sp == ':' || *sp == ' ') sp++;

                if(strncmp(sp, "OK", 2) == 0) {
                    MfpSectorResult* sr = &app->sector_results[cur_sector];
                    sr->status = MfpSectorOk;
                    sr->key_a_found = false;
                    sr->key_b_found = false;
                    sp += 2;

                    /* Look for "KeyA <32hex>" and/or "KeyB <32hex>" tokens
                     * anywhere on the line. Order-independent. */
                    while(*sp) {
                        while(*sp == ' ') sp++;
                        if(!*sp) break;
                        MfpKey* target = NULL;
                        bool* found_flag = NULL;
                        if(strncmp(sp, "KeyA", 4) == 0) {
                            target = &sr->key_a;
                            found_flag = &sr->key_a_found;
                            sp += 4;
                        } else if(strncmp(sp, "KeyB", 4) == 0) {
                            target = &sr->key_b;
                            found_flag = &sr->key_b_found;
                            sp += 4;
                        } else {
                            /* Unknown token — skip one word to avoid loop */
                            while(*sp && *sp != ' ') sp++;
                            continue;
                        }
                        while(*sp == ' ') sp++;
                        for(uint8_t k = 0; k < MFP_AES_KEY_SIZE && sp[0] && sp[1]; k++) {
                            char byte_str[3] = {sp[0], sp[1], '\0'};
                            (*target)[k] = (uint8_t)strtoul(byte_str, NULL, 16);
                            sp += 2;
                        }
                        *found_flag = true;
                    }
                    in_sector = true;
                } else if(strncmp(sp, "AuthFail", 8) == 0) {
                    app->sector_results[cur_sector].status = MfpSectorAuthFail;
                    in_sector = false;
                } else if(strncmp(sp, "ReadFail", 8) == 0) {
                    app->sector_results[cur_sector].status = MfpSectorReadFail;
                    in_sector = false;
                }
            }
        } else if(strncmp(p, "Block ", 6) == 0 && in_sector) {
            /* Parse "Block N: XX XX XX ..." */
            const char* sp = p + 6;
            uint16_t blk_num = (uint16_t)strtoul(sp, (char**)&sp, 10);
            if(blk_num < MFP_MAX_BLOCKS) {
                const char* col = strchr(sp, ':');
                if(col) {
                    col++;
                    uint8_t j = 0;
                    while(*col && j < MFP_BLOCK_SIZE) {
                        while(*col == ' ') col++;
                        if(!*col) break;
                        app->blocks[blk_num][j++] =
                            (uint8_t)strtoul(col, (char**)&col, 16);
                    }
                    if(cur_sector < MFP_SECTORS_4K) {
                        app->sector_results[cur_sector].blocks_read++;
                    }
                }
            }
        }

        p = end ? end + 1 : NULL;
    }

    app->read_all_mode = true;
    app->scan_total_sectors = mfp_sector_count(app->version.size);
}

bool mfp_storage_load(MfpApp* app, const char* path) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false;
    }

    uint64_t size = storage_file_size(f);
    /* V2 files can be much larger */
    if(size > 32 * 1024) size = 32 * 1024;

    char* buf = malloc((size_t)size + 1);
    if(!buf) {
        storage_file_close(f);
        storage_file_free(f);
        return false;
    }

    size_t got = storage_file_read(f, buf, (size_t)size);
    buf[got] = '\0';
    storage_file_close(f);
    storage_file_free(f);

    /* Reset app state */
    memset(&app->version, 0, sizeof(app->version));
    memset(app->sector_results, 0, sizeof(app->sector_results));
    app->blocks_read = 0;
    app->target_sector = 0;
    app->read_all_mode = false;
    app->scan_sectors_ok = 0;
    app->scan_sectors_done = 0;
    app->scan_total_sectors = 0;
    app->loaded_from_file = true;
    /* Default: legacy files without the field allow overwrite */
    app->allow_overwrite = true;

    /* Detect version */
    int version = 1;
    const char* vl = strstr(buf, "Version: ");
    if(vl) {
        version = atoi(vl + 9);
    }

    if(version >= 2) {
        mfp_storage_parse_v2(app, buf);
    } else {
        mfp_storage_parse_v1(app, buf);
    }

    free(buf);
    strlcpy(app->save_path, path, sizeof(app->save_path));
    return app->version.uid_len > 0;
}
