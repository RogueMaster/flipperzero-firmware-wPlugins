/*
 * GhostBook v0.6.0
 * Secure encrypted contacts for Flipper Zero
 * 
 * Original Author: Digi
 * Created: December 2025
 * License: MIT
 * 
 * If you fork this, keep this header intact.
 * https://github.com/digitard/ghostbook
 */

#include "ghostbook.h"

// Embedded signature - survives in compiled binary
static const char* const GHOSTBOOK_SIGNATURE =
    "GhostBook v0.6.0 | Original Author: Digi | Dec 2025 | github.com/digitard/ghostbook";

// Reference to prevent compiler from optimizing out the signature
__attribute__((used)) static const char* ghost_get_signature(void) {
    return GHOSTBOOK_SIGNATURE;
}

// ============================================================================
// CONSTANTS
// ============================================================================

static const char* flair_names[] =
    {"None", "Skull", "Ghost", "Matrix", "Glitch", "Lock", "Eye", "Bolt", "Star"};
static const char* flair_icons[] =
    {"", "(x_x)", "(o_o)", "[01]", "~#!~", "[=]", "(O)", "/!\\", "(*)"};
static const char* button_names[] = {"UP", "DOWN", "LEFT", "RIGHT", "OK", "BACK"};

const char* ghost_flair_name(GhostFlair f) {
    return (f < GhostFlairNum) ? flair_names[f] : flair_names[0];
}
const char* ghost_flair_icon(GhostFlair f) {
    return (f < GhostFlairNum) ? flair_icons[f] : flair_icons[0];
}
const char* ghost_button_name(PasscodeButton b) {
    return (b <= PasscodeBack) ? button_names[b] : "?";
}

// ============================================================================
// SIMPLE HASH FUNCTION (FNV-1a based, iterated for security)
// ============================================================================

static void ghost_hash(const uint8_t* data, size_t len, uint8_t* hash_out) {
    // FNV-1a 256-bit variant, iterated for key stretching
    uint64_t h[4] = {
        0xcbf29ce484222325ULL,
        0xcbf29ce484222325ULL ^ 0x100,
        0xcbf29ce484222325ULL ^ 0x200,
        0xcbf29ce484222325ULL ^ 0x300};

    // Hash the data
    for(size_t i = 0; i < len; i++) {
        for(int j = 0; j < 4; j++) {
            h[j] ^= data[i];
            h[j] *= 0x100000001b3ULL;
            h[j] ^= (h[j] >> 33);
        }
    }

    // Mix between lanes
    h[0] += h[1] * 31;
    h[1] += h[2] * 37;
    h[2] += h[3] * 41;
    h[3] += h[0] * 43;

    // Output 32 bytes
    memcpy(hash_out, h, 32);
}

void ghost_derive_key(
    const uint8_t* passcode,
    uint8_t passcode_len,
    const uint8_t* salt,
    uint8_t* key_out) {
    // Combine passcode and salt
    uint8_t combined[MAX_PASSCODE_LENGTH + SALT_LENGTH];
    memcpy(combined, salt, SALT_LENGTH);
    memcpy(combined + SALT_LENGTH, passcode, passcode_len);

    // Initial hash
    ghost_hash(combined, SALT_LENGTH + passcode_len, key_out);

    // Key stretching - iterate KEY_ITERATIONS times (10,000)
    uint8_t temp[32 + SALT_LENGTH];
    for(int i = 0; i < KEY_ITERATIONS; i++) {
        memcpy(temp, key_out, 32);
        memcpy(temp + 32, salt, SALT_LENGTH);
        temp[0] ^= (i & 0xFF);
        temp[1] ^= ((i >> 8) & 0xFF);
        ghost_hash(temp, sizeof(temp), key_out);
    }
}

bool ghost_encrypt_data(
    const uint8_t* key,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* cipher,
    size_t* cipher_len) {
    // Generate random IV
    uint8_t iv[IV_LENGTH];
    furi_hal_random_fill_buf(iv, IV_LENGTH);

    // Output: IV + encrypted data
    memcpy(cipher, iv, IV_LENGTH);

    // Stream cipher using hash-based keystream
    uint8_t keystream[32];
    uint8_t block_input[48];

    for(size_t i = 0; i < plain_len; i++) {
        if(i % 32 == 0) {
            // Generate new keystream block
            memcpy(block_input, key, KEY_LENGTH);
            memcpy(block_input + KEY_LENGTH, iv, IV_LENGTH);
            block_input[KEY_LENGTH + IV_LENGTH - 1] ^= (i / 32);
            ghost_hash(block_input, KEY_LENGTH + IV_LENGTH, keystream);
        }
        cipher[IV_LENGTH + i] = plain[i] ^ keystream[i % 32];
    }

    *cipher_len = IV_LENGTH + plain_len;
    return true;
}

bool ghost_decrypt_data(
    const uint8_t* key,
    const uint8_t* cipher,
    size_t cipher_len,
    uint8_t* plain,
    size_t* plain_len) {
    if(cipher_len < IV_LENGTH) return false;

    // Extract IV
    uint8_t iv[IV_LENGTH];
    memcpy(iv, cipher, IV_LENGTH);

    size_t data_len = cipher_len - IV_LENGTH;

    // Stream cipher decryption
    uint8_t keystream[32];
    uint8_t block_input[48];

    for(size_t i = 0; i < data_len; i++) {
        if(i % 32 == 0) {
            memcpy(block_input, key, KEY_LENGTH);
            memcpy(block_input + KEY_LENGTH, iv, IV_LENGTH);
            block_input[KEY_LENGTH + IV_LENGTH - 1] ^= (i / 32);
            ghost_hash(block_input, KEY_LENGTH + IV_LENGTH, keystream);
        }
        plain[i] = cipher[IV_LENGTH + i] ^ keystream[i % 32];
    }

    *plain_len = data_len;
    return true;
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

bool ghost_auth_load(GhostApp* app) {
    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    memset(&app->auth, 0, sizeof(GhostAuth));

    if(storage_file_open(f, GHOSTBOOK_AUTH_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(f, &app->auth, sizeof(GhostAuth)) == sizeof(GhostAuth)) {
            ok = app->auth.initialized;
            // Backward compatibility: default max_attempts if not set
            if(ok && app->auth.max_attempts == 0) {
                app->auth.max_attempts = DEFAULT_MAX_ATTEMPTS;
            }
        }
        storage_file_close(f);
    }

    storage_file_free(f);
    return ok;
}

bool ghost_auth_save(GhostApp* app) {
    storage_simply_mkdir(app->storage, GHOSTBOOK_FOLDER);

    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, GHOSTBOOK_AUTH_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(f, &app->auth, sizeof(GhostAuth)) == sizeof(GhostAuth);
        storage_file_close(f);
    }

    storage_file_free(f);
    return ok;
}

bool ghost_auth_verify(GhostApp* app, const uint8_t* passcode) {
    uint8_t len = app->auth.passcode_length;

    // Hash the input passcode with stored salt
    uint8_t hash_input[SALT_LENGTH + MAX_PASSCODE_LENGTH];
    memcpy(hash_input, app->auth.salt, SALT_LENGTH);
    memcpy(hash_input + SALT_LENGTH, passcode, len);

    uint8_t hash[32];
    ghost_hash(hash_input, SALT_LENGTH + len, hash);

    // Compare with stored hash
    if(memcmp(hash, app->auth.passcode_hash, 32) == 0) {
        // Success - reset failed attempts and derive key
        app->auth.failed_attempts = 0;
        ghost_auth_save(app);
        ghost_derive_key(passcode, len, app->auth.salt, app->derived_key);
        app->unlocked = true;
        return true;
    } else {
        // Failed attempt
        app->auth.failed_attempts++;
        ghost_auth_save(app);
        return false;
    }
}

void ghost_auth_set(GhostApp* app, const uint8_t* passcode, uint8_t length, uint8_t max_attempts) {
    // Generate new salt
    furi_hal_random_fill_buf(app->auth.salt, SALT_LENGTH);

    // Store passcode length and max attempts
    app->auth.passcode_length = length;
    app->auth.max_attempts = max_attempts;

    // Hash passcode with salt
    uint8_t hash_input[SALT_LENGTH + MAX_PASSCODE_LENGTH];
    memcpy(hash_input, app->auth.salt, SALT_LENGTH);
    memcpy(hash_input + SALT_LENGTH, passcode, length);
    ghost_hash(hash_input, SALT_LENGTH + length, app->auth.passcode_hash);

    app->auth.failed_attempts = 0;
    app->auth.initialized = true;

    ghost_auth_save(app);

    // Derive encryption key
    ghost_derive_key(passcode, length, app->auth.salt, app->derived_key);
    app->unlocked = true;
}

void ghost_wipe_all_data(GhostApp* app) {
    FURI_LOG_W(TAG, "!!! WIPING ALL DATA !!!");

    // Delete auth file
    storage_simply_remove(app->storage, GHOSTBOOK_AUTH_FILE);

    // Delete profile
    storage_simply_remove(app->storage, GHOSTBOOK_PROFILE);

    // Delete all contacts
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, GHOSTBOOK_CONTACTS_DIR)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(!(info.flags & FSF_DIRECTORY)) {
                FuriString* path = furi_string_alloc();
                furi_string_printf(path, "%s/%s", GHOSTBOOK_CONTACTS_DIR, name);
                storage_simply_remove(app->storage, furi_string_get_cstr(path));
                furi_string_free(path);
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    // Remove contacts directory
    storage_simply_remove(app->storage, GHOSTBOOK_CONTACTS_DIR);

    // Clear memory
    memset(&app->auth, 0, sizeof(GhostAuth));
    memset(&app->my_card, 0, sizeof(GhostCard));
    memset(app->derived_key, 0, KEY_LENGTH);
    ghost_free_contacts(app);
    app->unlocked = false;

    notification_message(app->notif, &sequence_error);
}

// ============================================================================
// CARD OPERATIONS (with encryption)
// ============================================================================

void ghost_card_init(GhostCard* c) {
    memset(c, 0, sizeof(GhostCard));
    c->flair = GhostFlairNone;
}

static void ghost_card_to_string(GhostCard* c, FuriString* s) {
    furi_string_printf(
        s,
        "GHOST:1\nHANDLE:%s\nNAME:%s\nEMAIL:%s\nDISCORD:%s\nSIGNAL:%s\nTELEGRAM:%s\nNOTES:%s\nFLAIR:%d\nEND\n",
        c->handle,
        c->name,
        c->email,
        c->discord,
        c->signal,
        c->telegram,
        c->notes,
        c->flair);
}

static bool ghost_card_from_string(const char* data, GhostCard* c) {
    ghost_card_init(c);
    if(strncmp(data, "GHOST:1", 7) != 0) return false;

    const char* p = data;
    char line[256];
    while(*p) {
        const char* nl = strchr(p, '\n');
        if(!nl) break;
        size_t len = nl - p;
        if(len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';

        if(strncmp(line, "HANDLE:", 7) == 0)
            strncpy(c->handle, line + 7, GHOST_HANDLE_LEN - 1);
        else if(strncmp(line, "NAME:", 5) == 0)
            strncpy(c->name, line + 5, GHOST_NAME_LEN - 1);
        else if(strncmp(line, "EMAIL:", 6) == 0)
            strncpy(c->email, line + 6, GHOST_EMAIL_LEN - 1);
        else if(strncmp(line, "DISCORD:", 8) == 0)
            strncpy(c->discord, line + 8, GHOST_DISCORD_LEN - 1);
        else if(strncmp(line, "SIGNAL:", 7) == 0)
            strncpy(c->signal, line + 7, GHOST_SIGNAL_LEN - 1);
        else if(strncmp(line, "TELEGRAM:", 9) == 0)
            strncpy(c->telegram, line + 9, GHOST_TELEGRAM_LEN - 1);
        else if(strncmp(line, "NOTES:", 6) == 0)
            strncpy(c->notes, line + 6, GHOST_NOTES_LEN - 1);
        else if(strncmp(line, "FLAIR:", 6) == 0)
            c->flair = atoi(line + 6);
        else if(strcmp(line, "END") == 0)
            return strlen(c->handle) > 0;
        p = nl + 1;
    }
    return strlen(c->handle) > 0;
}

bool ghost_save_profile(GhostApp* app) {
    if(!app->unlocked) return false;

    storage_simply_mkdir(app->storage, GHOSTBOOK_FOLDER);

    // Serialize card
    FuriString* s = furi_string_alloc();
    ghost_card_to_string(&app->my_card, s);

    // Encrypt
    size_t plain_len = furi_string_size(s);
    size_t cipher_len;
    uint8_t* cipher = malloc(plain_len + IV_LENGTH + 32);

    bool ok = false;
    if(cipher &&
       ghost_encrypt_data(
           app->derived_key, (uint8_t*)furi_string_get_cstr(s), plain_len, cipher, &cipher_len)) {
        File* f = storage_file_alloc(app->storage);
        if(storage_file_open(f, GHOSTBOOK_PROFILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            ok = storage_file_write(f, cipher, cipher_len) == cipher_len;
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(cipher) free(cipher);
    furi_string_free(s);
    return ok;
}

bool ghost_load_profile(GhostApp* app) {
    if(!app->unlocked) return false;

    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, GHOSTBOOK_PROFILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > IV_LENGTH && sz < 4096) {
            uint8_t* cipher = malloc(sz);
            uint8_t* plain = malloc(sz);

            if(cipher && plain) {
                storage_file_read(f, cipher, sz);
                size_t plain_len;

                if(ghost_decrypt_data(app->derived_key, cipher, sz, plain, &plain_len)) {
                    plain[plain_len] = '\0';
                    ok = ghost_card_from_string((char*)plain, &app->my_card);
                }
            }

            if(cipher) free(cipher);
            if(plain) free(plain);
        }
        storage_file_close(f);
    }

    storage_file_free(f);

    if(!ok) {
        ghost_card_init(&app->my_card);
        strncpy(app->my_card.handle, "ghost", GHOST_HANDLE_LEN - 1);
    }
    return ok;
}

bool ghost_save_contact(GhostApp* app, GhostCard* c) {
    if(!app->unlocked) return false;

    storage_simply_mkdir(app->storage, GHOSTBOOK_CONTACTS_DIR);

    FuriString* s = furi_string_alloc();
    ghost_card_to_string(c, s);

    size_t plain_len = furi_string_size(s);
    size_t cipher_len;
    uint8_t* cipher = malloc(plain_len + IV_LENGTH + 32);

    bool ok = false;
    if(cipher &&
       ghost_encrypt_data(
           app->derived_key, (uint8_t*)furi_string_get_cstr(s), plain_len, cipher, &cipher_len)) {
        FuriString* path = furi_string_alloc();
        furi_string_printf(
            path, "%s/%s_%lu.enc", GHOSTBOOK_CONTACTS_DIR, c->handle, furi_hal_rtc_get_timestamp());

        File* f = storage_file_alloc(app->storage);
        if(storage_file_open(f, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            ok = storage_file_write(f, cipher, cipher_len) == cipher_len;
            storage_file_close(f);
        }
        storage_file_free(f);
        furi_string_free(path);
    }

    if(cipher) free(cipher);
    furi_string_free(s);
    return ok;
}

bool ghost_load_contacts(GhostApp* app) {
    if(!app->unlocked) return false;

    ghost_free_contacts(app);

    File* dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, GHOSTBOOK_CONTACTS_DIR)) {
        storage_file_free(dir);
        return false;
    }

    // Count files
    FileInfo info;
    char name[128];
    uint32_t count = 0;
    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(!(info.flags & FSF_DIRECTORY) && strstr(name, ".enc")) count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);

    if(count == 0) {
        return true;
    }

    app->contacts = malloc(count * sizeof(GhostContact));
    if(!app->contacts) {
        return false;
    }

    // Reopen directory for second pass
    dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, GHOSTBOOK_CONTACTS_DIR)) {
        storage_file_free(dir);
        free(app->contacts);
        app->contacts = NULL;
        return false;
    }

    uint32_t loaded = 0;

    while(storage_dir_read(dir, &info, name, sizeof(name)) && loaded < count) {
        if(!(info.flags & FSF_DIRECTORY) && strstr(name, ".enc")) {
            FuriString* path = furi_string_alloc();
            furi_string_printf(path, "%s/%s", GHOSTBOOK_CONTACTS_DIR, name);

            File* f = storage_file_alloc(app->storage);
            if(storage_file_open(f, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
                uint64_t sz = storage_file_size(f);
                if(sz > IV_LENGTH && sz < 4096) {
                    uint8_t* cipher = malloc(sz);
                    uint8_t* plain = malloc(sz);

                    if(cipher && plain) {
                        storage_file_read(f, cipher, sz);
                        size_t plain_len;

                        if(ghost_decrypt_data(app->derived_key, cipher, sz, plain, &plain_len)) {
                            plain[plain_len] = '\0';
                            if(ghost_card_from_string((char*)plain, &app->contacts[loaded].card)) {
                                strncpy(app->contacts[loaded].filename, name, 63);
                                loaded++;
                            }
                        }
                    }

                    if(cipher) free(cipher);
                    if(plain) free(plain);
                }
                storage_file_close(f);
            }
            storage_file_free(f);
            furi_string_free(path);
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    app->contact_count = loaded;
    return true;
}

void ghost_free_contacts(GhostApp* app) {
    if(app->contacts) {
        free(app->contacts);
        app->contacts = NULL;
    }
    app->contact_count = 0;
}

// ============================================================================
// EXPORT (decrypted output - only works when unlocked)
// ============================================================================

bool ghost_export_vcard(GhostApp* app, GhostCard* c, const char* path) {
    if(!app->unlocked) return false;

    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* v = furi_string_alloc();
        furi_string_printf(v, "BEGIN:VCARD\r\nVERSION:3.0\r\nNICKNAME:%s\r\n", c->handle);
        if(strlen(c->name)) furi_string_cat_printf(v, "FN:%s\r\n", c->name);
        if(strlen(c->email)) furi_string_cat_printf(v, "EMAIL:%s\r\n", c->email);
        if(strlen(c->discord)) furi_string_cat_printf(v, "X-DISCORD:%s\r\n", c->discord);
        if(strlen(c->signal)) furi_string_cat_printf(v, "X-SIGNAL:%s\r\n", c->signal);
        if(strlen(c->telegram)) furi_string_cat_printf(v, "X-TELEGRAM:%s\r\n", c->telegram);
        if(strlen(c->notes)) furi_string_cat_printf(v, "NOTE:%s\r\n", c->notes);
        furi_string_cat_printf(v, "END:VCARD\r\n");
        ok = storage_file_write(f, furi_string_get_cstr(v), furi_string_size(v)) ==
             furi_string_size(v);
        furi_string_free(v);
        storage_file_close(f);
    }
    storage_file_free(f);
    return ok;
}

bool ghost_export_all_vcard(GhostApp* app, const char* path) {
    if(!app->unlocked || app->contact_count == 0) return false;

    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* v = furi_string_alloc();
        for(uint32_t i = 0; i < app->contact_count; i++) {
            GhostCard* c = &app->contacts[i].card;
            furi_string_cat_printf(v, "BEGIN:VCARD\r\nVERSION:3.0\r\nNICKNAME:%s\r\n", c->handle);
            if(strlen(c->name)) furi_string_cat_printf(v, "FN:%s\r\n", c->name);
            if(strlen(c->email)) furi_string_cat_printf(v, "EMAIL:%s\r\n", c->email);
            if(strlen(c->discord)) furi_string_cat_printf(v, "X-DISCORD:%s\r\n", c->discord);
            if(strlen(c->signal)) furi_string_cat_printf(v, "X-SIGNAL:%s\r\n", c->signal);
            if(strlen(c->telegram)) furi_string_cat_printf(v, "X-TELEGRAM:%s\r\n", c->telegram);
            if(strlen(c->notes)) furi_string_cat_printf(v, "NOTE:%s\r\n", c->notes);
            furi_string_cat_printf(v, "END:VCARD\r\n\r\n");
        }
        ok = storage_file_write(f, furi_string_get_cstr(v), furi_string_size(v)) ==
             furi_string_size(v);
        furi_string_free(v);
        storage_file_close(f);
    }
    storage_file_free(f);
    return ok;
}

bool ghost_export_csv(GhostApp* app, const char* path) {
    if(!app->unlocked) return false;

    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* csv = furi_string_alloc();
        furi_string_printf(csv, "Handle,Name,Email,Discord,Signal,Telegram,Notes\r\n");
        furi_string_cat_printf(
            csv,
            "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\r\n",
            app->my_card.handle,
            app->my_card.name,
            app->my_card.email,
            app->my_card.discord,
            app->my_card.signal,
            app->my_card.telegram,
            app->my_card.notes);
        for(uint32_t i = 0; i < app->contact_count; i++) {
            GhostCard* c = &app->contacts[i].card;
            furi_string_cat_printf(
                csv,
                "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\r\n",
                c->handle,
                c->name,
                c->email,
                c->discord,
                c->signal,
                c->telegram,
                c->notes);
        }
        ok = storage_file_write(f, furi_string_get_cstr(csv), furi_string_size(csv)) ==
             furi_string_size(csv);
        furi_string_free(csv);
        storage_file_close(f);
    }
    storage_file_free(f);
    return ok;
}

// ============================================================================
// VIEW DISPATCHER CALLBACKS
// ============================================================================

typedef enum {
    GhostCustomEventPasscodeComplete = 100,
    GhostCustomEventPopupDone,
} GhostCustomEvent;

static void ghost_handle_passcode_complete(GhostApp* app) {
    switch(app->current_scene) {
    case GhostSceneLock: {
        // Verify passcode
        if(ghost_auth_verify(app, app->passcode_input)) {
            // Success!
            notification_message(app->notif, &sequence_success);
            ghost_load_profile(app);
            ghost_scene_main_enter(app);
        } else {
            // Wrong passcode
            notification_message(app->notif, &sequence_error);

            if(app->auth.failed_attempts >= app->auth.max_attempts) {
                // WIPE EVERYTHING
                ghost_wipe_all_data(app);
                ghost_scene_wiped_enter(app);
            } else {
                // Show warning popup then return to lock
                popup_reset(app->popup);
                popup_set_header(app->popup, "WRONG CODE", 64, 15, AlignCenter, AlignTop);

                char msg[64];
                snprintf(
                    msg,
                    sizeof(msg),
                    "%d attempts left!\nData WIPED at 0",
                    app->auth.max_attempts - app->auth.failed_attempts);
                popup_set_text(app->popup, msg, 64, 32, AlignCenter, AlignTop);
                popup_set_timeout(app->popup, 2000);
                popup_enable_timeout(app->popup);
                popup_set_callback(app->popup, NULL);
                view_dispatcher_switch_to_view(app->view, GhostViewPopup);

                // Reset and return to lock after delay
                furi_delay_ms(2000);
                app->passcode_pos = 0;
                memset(app->passcode_input, 0, MAX_PASSCODE_LENGTH);
                view_dispatcher_switch_to_view(app->view, GhostViewPasscode);
            }
        }
        break;
    }

    case GhostSceneSetup: {
        // Save first entry and move to confirm
        memcpy(app->setup_passcode, app->passcode_input, app->setup_length);
        notification_message(app->notif, &sequence_blink_green_10);
        ghost_scene_setup_confirm_enter(app);
        break;
    }

    case GhostSceneSetupConfirm: {
        // Check if matches
        if(memcmp(app->setup_passcode, app->passcode_input, app->setup_length) == 0) {
            // Match! Set up auth
            ghost_auth_set(app, app->passcode_input, app->setup_length, app->setup_max_attempts);
            notification_message(app->notif, &sequence_success);

            // Initialize default profile
            ghost_card_init(&app->my_card);
            strncpy(app->my_card.handle, "ghost", GHOST_HANDLE_LEN - 1);
            ghost_save_profile(app);

            ghost_scene_main_enter(app);
        } else {
            // No match - start over
            notification_message(app->notif, &sequence_error);

            popup_reset(app->popup);
            popup_set_header(app->popup, "NO MATCH", 64, 20, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Codes didn't match\nTry again", 64, 35, AlignCenter, AlignTop);
            popup_set_timeout(app->popup, 1500);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, NULL);
            view_dispatcher_switch_to_view(app->view, GhostViewPopup);

            furi_delay_ms(1500);
            memset(app->setup_passcode, 0, MAX_PASSCODE_LENGTH);
            ghost_scene_setup_enter(app);
        }
        break;
    }

    default:
        break;
    }
}

static bool custom_cb(void* ctx, uint32_t event) {
    GhostApp* app = ctx;

    // Passcode complete event
    if(event == GhostCustomEventPasscodeComplete) {
        ghost_handle_passcode_complete(app);
        return true;
    }

    // Menu events - these match GhostEv enum in scenes file
    switch(event) {
    // Main menu events
    case 2:
        ghost_scene_view_enter(app);
        return true; // EvView
    case 3:
        ghost_scene_edit_enter(app);
        return true; // EvEdit
    case 10:
        ghost_scene_share_enter(app);
        return true; // EvShare
    case 11:
        ghost_scene_receive_enter(app);
        return true; // EvReceive
    case 12:
        ghost_scene_contacts_enter(app);
        return true; // EvContacts
    case 14:
        ghost_scene_export_enter(app);
        return true; // EvExport
    case 15:
        ghost_scene_about_enter(app);
        return true; // EvAbout

    // Edit menu events - go to text input
    case 4: // EvHandle
    case 5: // EvName
    case 6: // EvEmail
    case 7: // EvDiscord
    case 8: // EvSignal
    case 9: // EvNotes
        app->edit_field = event - 4;
        ghost_scene_text_enter(app);
        return true;
    case 13: // EvFlair
        ghost_scene_flair_enter(app);
        return true;

    // Text input done - save the data
    case 17: { // EvTextDone
        GhostCard* c = &app->my_card;
        switch(app->edit_field) {
        case 0:
            strncpy(c->handle, app->text_buf, GHOST_HANDLE_LEN - 1);
            break;
        case 1:
            strncpy(c->name, app->text_buf, GHOST_NAME_LEN - 1);
            break;
        case 2:
            strncpy(c->email, app->text_buf, GHOST_EMAIL_LEN - 1);
            break;
        case 3:
            strncpy(c->discord, app->text_buf, GHOST_DISCORD_LEN - 1);
            break;
        case 4:
            strncpy(c->signal, app->text_buf, GHOST_SIGNAL_LEN - 1);
            break;
        case 5:
            strncpy(c->notes, app->text_buf, GHOST_NOTES_LEN - 1);
            break;
        }
        ghost_scene_edit_enter(app);
        return true;
    }

    // Flair done
    case 18: // EvFlairDone
        ghost_scene_edit_enter(app);
        return true;

    // Contact view
    case 16: // EvContactView
        ghost_scene_contact_view_enter(app);
        return true;
    }

    return false;
}

static bool back_cb(void* ctx) {
    GhostApp* app = ctx;

    switch(app->current_scene) {
    case GhostSceneMain:
        // Exit app from main menu
        view_dispatcher_stop(app->view);
        return true;

    case GhostSceneSecuritySetup:
    case GhostSceneLock:
    case GhostSceneWiped:
        // Exit app from these screens
        view_dispatcher_stop(app->view);
        return true;

    case GhostSceneSetup:
        // Go back to security selection
        ghost_scene_security_setup_enter(app);
        return true;

    case GhostSceneWipeSetup:
        // Go back to security level selection
        ghost_scene_security_setup_enter(app);
        return true;

    case GhostSceneSetupConfirm:
        // Go back to passcode entry
        ghost_scene_setup_enter(app);
        return true;

    case GhostSceneView:
    case GhostSceneContacts:
    case GhostSceneExport:
    case GhostSceneAbout:
        // Go back to main menu
        ghost_scene_main_enter(app);
        return true;

    case GhostSceneShare:
        // Stop NFC share and go back
        ghost_nfc_share_stop(app);
        notification_message(app->notif, &sequence_blink_stop);
        ghost_scene_main_enter(app);
        return true;

    case GhostSceneReceive:
        // Stop NFC receive and go back
        ghost_nfc_receive_stop(app);
        notification_message(app->notif, &sequence_blink_stop);
        ghost_scene_main_enter(app);
        return true;

    case GhostSceneEdit:
        // Save and go back to main
        ghost_save_profile(app);
        ghost_scene_main_enter(app);
        return true;

    case GhostSceneEditHandle:
    case GhostSceneEditName:
    case GhostSceneEditEmail:
    case GhostSceneEditDiscord:
    case GhostSceneEditSignal:
    case GhostSceneEditNotes:
    case GhostSceneFlair:
        // Go back to edit menu
        ghost_scene_edit_enter(app);
        return true;

    case GhostSceneContactView:
        // Go back to contacts list
        ghost_scene_contacts_enter(app);
        return true;

    default:
        ghost_scene_main_enter(app);
        return true;
    }
}

// ============================================================================
// APP LIFECYCLE
// ============================================================================

static GhostApp* ghost_alloc(void) {
    GhostApp* app = malloc(sizeof(GhostApp));
    memset(app, 0, sizeof(GhostApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notif = furi_record_open(RECORD_NOTIFICATION);

    app->view = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view, app);
    view_dispatcher_set_custom_event_callback(app->view, custom_cb);
    view_dispatcher_set_navigation_event_callback(app->view, back_cb);
    view_dispatcher_attach_to_gui(app->view, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view, GhostViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view, GhostViewWidget, widget_get_view(app->widget));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view, GhostViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view, GhostViewPopup, popup_get_view(app->popup));

    app->passcode_view = ghost_passcode_view_alloc(app);
    view_dispatcher_add_view(app->view, GhostViewPasscode, app->passcode_view);

    // NFC init
    app->nfc = NULL;
    app->nfc_listener = NULL;
    app->nfc_poller = NULL;
    app->nfc_active = false;
    app->card_received = false;

    app->unlocked = false;

    return app;
}

static void ghost_free(GhostApp* app) {
    // Stop any active NFC
    if(app->nfc_active) {
        ghost_nfc_share_stop(app);
        ghost_nfc_receive_stop(app);
    }

    // Clear sensitive data
    memset(app->derived_key, 0, KEY_LENGTH);
    memset(app->passcode_input, 0, MAX_PASSCODE_LENGTH);
    memset(app->setup_passcode, 0, MAX_PASSCODE_LENGTH);

    if(app->unlocked) {
        ghost_save_profile(app);
    }
    ghost_free_contacts(app);

    view_dispatcher_remove_view(app->view, GhostViewSubmenu);
    view_dispatcher_remove_view(app->view, GhostViewWidget);
    view_dispatcher_remove_view(app->view, GhostViewTextInput);
    view_dispatcher_remove_view(app->view, GhostViewPopup);
    view_dispatcher_remove_view(app->view, GhostViewPasscode);

    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    popup_free(app->popup);
    ghost_passcode_view_free(app->passcode_view);

    view_dispatcher_free(app->view);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// ============================================================================
// ENTRY POINT
// ============================================================================

int32_t ghostbook_app(void* p) {
    UNUSED(p);

    GhostApp* app = ghost_alloc();

    // Check if passcode is set up
    if(ghost_auth_load(app)) {
        // Show lock screen
        ghost_scene_lock_enter(app);
    } else {
        // First run - choose security level first
        ghost_scene_security_setup_enter(app);
    }

    view_dispatcher_run(app->view);
    ghost_free(app);

    return 0;
}
