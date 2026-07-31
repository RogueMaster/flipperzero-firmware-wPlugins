#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ck42x_passvault_icons.h>

#include "ck42x_fido2_service.h"

#define CK_TAG            "CK42XPassVault"
#define CK_MAX_ENTRIES    20
#define CK_ACCOUNT_LEN    32
#define CK_USERNAME_LEN   48
#define CK_PASSWORD_LEN   72
#define CK_INPUT_LEN      72
#define CK_LINE_MAX       180
#define CK_PIN_MIN_LEN    4
#define CK_MAX_VAULT_FILE 8192

#define CK_PLAINTEXT_FILE APP_DATA_PATH("vault.tsv")
#define CK_VAULT_FILE     APP_DATA_PATH("vault.pv1")

#define CK_KEY_LEN   32
#define CK_SALT_LEN  16
#define CK_NONCE_LEN 12
#define CK_TAG_LEN   16
#define CK_MAGIC_LEN 8
#define CK_HDR_LEN   (CK_MAGIC_LEN + CK_SALT_LEN + CK_NONCE_LEN + CK_TAG_LEN)

static const uint8_t ck_vault_magic[CK_MAGIC_LEN] = {'C', 'K', 'P', 'V', '1', 0, 0, 0};
typedef struct {
    char account[CK_ACCOUNT_LEN];
    char username[CK_USERNAME_LEN];
    char password[CK_PASSWORD_LEN];
} CkVaultEntry;

typedef enum {
    CkViewMain = 0,
    CkViewTextInput,
    CkViewWidget,
    CkViewDialog,
} CkView;

typedef enum {
    CkMenuModeMain = 0,
    CkMenuModeGenerateOrCustom,
    CkMenuModePreset,
} CkMenuMode;

typedef enum {
    CkInputAccount = 0,
    CkInputUsername,
    CkInputCustomPassword,
    CkInputSetPin,
    CkInputUnlockPin,
} CkInputStage;

typedef enum {
    CkDialogSave = 0,
    CkDialogInjectConfirm,
    CkDialogFido2Presence,
} CkDialogPurpose;

typedef enum {
    CkEventAdd = 1,
    CkEventAbout = 2,
    CkEventSecurityKey = 3,
    CkEventSavedBase = 100,
    CkEventTextDone = 300,
    CkEventChooseGenerate = 400,
    CkEventChooseCustom = 401,
    CkEventPresetMemorable = 500,
    CkEventPresetStrict = 501,
    CkEventPresetLong = 502,
    CkEventPresetNoSymbol = 503,
    CkEventWidgetBack = 600,
    CkEventWidgetInject = 601,
    CkEventDialogRight = 700,
    CkEventDialogLeft = 701,
} CkEvent;

typedef enum {
    CkPresetMemorable = 0,
    CkPresetStrict,
    CkPresetLong,
    CkPresetNoSymbol,
} CkPreset;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    DialogEx* dialog;
    Storage* storage;

    CkVaultEntry entries[CK_MAX_ENTRIES];
    uint8_t entry_count;
    int8_t selected;

    CkVaultEntry draft;
    char input[CK_INPUT_LEN];
    CkView current_view;
    CkMenuMode menu_mode;
    CkInputStage input_stage;
    CkDialogPurpose dialog_purpose;
    FuriHalUsbInterface* previous_usb;
    CkFido2Service* fido2_service;
    bool security_key_view;

    uint8_t vault_key[CK_KEY_LEN];
    uint8_t vault_salt[CK_SALT_LEN];
    bool unlocked;
} CkApp;

static void ck_show_main(CkApp* app);
static void ck_show_text_input(CkApp* app, CkInputStage stage, const char* header, char* initial);
static void ck_show_save_dialog(CkApp* app);
static void ck_show_entry_widget(CkApp* app);
static void ck_show_about(CkApp* app);
static void ck_show_security_key(CkApp* app);
static void ck_show_fido2_presence(CkApp* app);
static void ck_stop_fido2(CkApp* app);
static void ck_show_inject_confirm(CkApp* app);
static void ck_handle_event(CkApp* app, uint32_t event);
static void ck_begin_auth(CkApp* app);

static void ck_copy(char* dst, size_t dst_size, const char* src) {
    if(dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static void ck_sanitize(char* text) {
    for(char* p = text; *p; p++) {
        if(*p == '\t' || *p == '\r' || *p == '\n') *p = ' ';
    }
}

static uint32_t ck_random_index(uint32_t max) {
    if(max == 0) return 0;
    return furi_hal_random_get() % max;
}

static void ck_secure_zero(void* ptr, size_t len) {
    volatile uint8_t* p = ptr;
    while(len--)
        *p++ = 0;
}

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} CkSha256Ctx;

static const uint32_t ck_sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

static uint32_t ck_rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void ck_sha256_transform(CkSha256Ctx* ctx, const uint8_t data[64]) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];

    for(uint8_t i = 0, j = 0; i < 16; i++, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) | (uint32_t)data[j + 3];
    }
    for(uint8_t i = 16; i < 64; i++) {
        uint32_t s0 = ck_rotr32(m[i - 15], 7) ^ ck_rotr32(m[i - 15], 18) ^ (m[i - 15] >> 3);
        uint32_t s1 = ck_rotr32(m[i - 2], 17) ^ ck_rotr32(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for(uint8_t i = 0; i < 64; i++) {
        uint32_t s1 = ck_rotr32(e, 6) ^ ck_rotr32(e, 11) ^ ck_rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        t1 = h + s1 + ch + ck_sha256_k[i] + m[i];
        uint32_t s0 = ck_rotr32(a, 2) ^ ck_rotr32(a, 13) ^ ck_rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void ck_sha256_init(CkSha256Ctx* ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void ck_sha256_update(CkSha256Ctx* ctx, const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        if(ctx->datalen == 64) {
            ck_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void ck_sha256_final(CkSha256Ctx* ctx, uint8_t hash[32]) {
    uint32_t i = ctx->datalen;

    ctx->data[i++] = 0x80;
    if(i > 56) {
        while(i < 64)
            ctx->data[i++] = 0x00;
        ck_sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while(i < 56)
        ctx->data[i++] = 0x00;

    ctx->bitlen += (uint64_t)ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    ck_sha256_transform(ctx, ctx->data);

    for(i = 0; i < 4; i++) {
        hash[i] = (ctx->state[0] >> (24 - i * 8)) & 0xff;
        hash[i + 4] = (ctx->state[1] >> (24 - i * 8)) & 0xff;
        hash[i + 8] = (ctx->state[2] >> (24 - i * 8)) & 0xff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
    }
}

static void ck_derive_key_from_pin(
    const char* pin,
    const uint8_t salt[CK_SALT_LEN],
    uint8_t key[CK_KEY_LEN]) {
    static const char domain[] = "CK42X-PassVault-KDF-v1";
    uint8_t block[32];
    uint8_t round_le[4];
    CkSha256Ctx ctx;
    size_t pin_len = strlen(pin);

    ck_sha256_init(&ctx);
    ck_sha256_update(&ctx, (const uint8_t*)domain, sizeof(domain) - 1);
    ck_sha256_update(&ctx, salt, CK_SALT_LEN);
    ck_sha256_update(&ctx, (const uint8_t*)pin, pin_len);
    ck_sha256_final(&ctx, block);

    for(uint32_t round = 0; round < 4096; round++) {
        round_le[0] = round;
        round_le[1] = round >> 8;
        round_le[2] = round >> 16;
        round_le[3] = round >> 24;
        ck_sha256_init(&ctx);
        ck_sha256_update(&ctx, (const uint8_t*)domain, sizeof(domain) - 1);
        ck_sha256_update(&ctx, block, sizeof(block));
        ck_sha256_update(&ctx, salt, CK_SALT_LEN);
        ck_sha256_update(&ctx, (const uint8_t*)pin, pin_len);
        ck_sha256_update(&ctx, round_le, sizeof(round_le));
        ck_sha256_final(&ctx, block);
    }

    memcpy(key, block, CK_KEY_LEN);
    ck_secure_zero(block, sizeof(block));
    ck_secure_zero(&ctx, sizeof(ctx));
}

static bool ck_password_exists(const CkApp* app, const char* password) {
    if(!password || password[0] == '\0') return true;
    for(uint8_t i = 0; i < app->entry_count; i++) {
        if(strcmp(app->entries[i].password, password) == 0) return true;
    }
    return false;
}

static void ck_make_password_unique(CkApp* app, char* password, size_t password_size) {
    if(!ck_password_exists(app, password)) return;

    char base[CK_PASSWORD_LEN];
    ck_copy(base, sizeof(base), password);

    for(uint32_t suffix = 0; suffix < 1000; suffix++) {
        snprintf(password, password_size, "%.*s%03lu", 60, base, (unsigned long)suffix);
        if(!ck_password_exists(app, password)) return;
    }
}

static const char* const ck_adjs[] = {
    "Amber",
    "Atomic",
    "Black",
    "Bright",
    "Cyber",
    "Drift",
    "Echo",
    "Iron",
    "Lunar",
    "Neon",
    "Nova",
    "Obsidian",
    "Rapid",
    "Solar",
    "Stone",
    "Velvet"};

static const char* const ck_nouns[] = {
    "Badger",
    "Falcon",
    "Harbor",
    "Mantis",
    "Otter",
    "Pioneer",
    "Raven",
    "River",
    "Rocket",
    "Signal",
    "Tiger",
    "Vector",
    "Wolf",
    "Anchor",
    "Forge",
    "Summit"};

static const char* const ck_symbols = "!@#$%&*?";
static const char* const ck_chars_upper = "ABCDEFGHJKLMNPQRSTUVWXYZ";
static const char* const ck_chars_lower = "abcdefghijkmnopqrstuvwxyz";
static const char* const ck_chars_digits = "23456789";

static char ck_pick_from(const char* alphabet) {
    return alphabet[ck_random_index(strlen(alphabet))];
}

static void ck_generate_password(CkPreset preset, char* out, size_t out_size) {
    const char* a1 = ck_adjs[ck_random_index(COUNT_OF(ck_adjs))];
    const char* a2 = ck_adjs[ck_random_index(COUNT_OF(ck_adjs))];
    const char* n1 = ck_nouns[ck_random_index(COUNT_OF(ck_nouns))];
    const char* n2 = ck_nouns[ck_random_index(COUNT_OF(ck_nouns))];
    uint32_t num = 10 + ck_random_index(90);
    char sym = ck_symbols[ck_random_index(strlen(ck_symbols))];

    if(preset == CkPresetMemorable) {
        snprintf(out, out_size, "%s%s%s%lu%c", a1, n1, n2, (unsigned long)num, sym);
    } else if(preset == CkPresetLong) {
        snprintf(
            out,
            out_size,
            "%s%s%s%lu%c",
            a1,
            n1,
            n2,
            (unsigned long)(100 + ck_random_index(900)),
            sym);
    } else if(preset == CkPresetNoSymbol) {
        snprintf(
            out, out_size, "%s%s%s%lu", a1, n1, a2, (unsigned long)(100 + ck_random_index(900)));
    } else {
        /* Strict mixed 16+ while still chunked enough to read aloud. */
        char tail[9];
        for(size_t i = 0; i < sizeof(tail) - 1; i++) {
            switch(i % 4) {
            case 0:
                tail[i] = ck_pick_from(ck_chars_upper);
                break;
            case 1:
                tail[i] = ck_pick_from(ck_chars_lower);
                break;
            case 2:
                tail[i] = ck_pick_from(ck_chars_digits);
                break;
            default:
                tail[i] = ck_pick_from(ck_symbols);
                break;
            }
        }
        tail[sizeof(tail) - 1] = '\0';
        snprintf(out, out_size, "%s%s%s", a1, n1, tail);
    }
}

static FuriString* ck_app_data_path(CkApp* app, const char* app_data_file) {
    FuriString* path = furi_string_alloc_set(app_data_file);
    storage_common_resolve_path_and_ensure_app_directory(app->storage, path);
    return path;
}

static bool ck_file_exists(CkApp* app, const char* app_data_file) {
    FuriString* path = ck_app_data_path(app, app_data_file);
    bool exists = storage_common_stat(app->storage, furi_string_get_cstr(path), NULL) == FSE_OK;
    furi_string_free(path);
    return exists;
}

static bool ck_remove_file(CkApp* app, const char* app_data_file) {
    FuriString* path = ck_app_data_path(app, app_data_file);
    bool ok = storage_simply_remove(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);
    return ok;
}

static void ck_parse_entries(CkApp* app, char* buf) {
    app->entry_count = 0;
    char* line = buf;
    while(line && *line && app->entry_count < CK_MAX_ENTRIES) {
        char* next = strchr(line, '\n');
        if(next) {
            *next = '\0';
            next++;
        }
        char* tab1 = strchr(line, '\t');
        if(tab1) {
            *tab1 = '\0';
            char* tab2 = strchr(tab1 + 1, '\t');
            if(tab2) {
                *tab2 = '\0';
                CkVaultEntry* e = &app->entries[app->entry_count++];
                ck_copy(e->account, sizeof(e->account), line);
                ck_copy(e->username, sizeof(e->username), tab1 + 1);
                ck_copy(e->password, sizeof(e->password), tab2 + 1);
            }
        }
        line = next;
    }
}

static bool ck_read_file(CkApp* app, const char* app_data_file, uint8_t** out, size_t* out_len) {
    bool ok = false;
    *out = NULL;
    *out_len = 0;
    FuriString* path = ck_app_data_path(app, app_data_file);
    File* file = storage_file_alloc(app->storage);

    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size <= CK_MAX_VAULT_FILE) {
            uint8_t* buf = malloc(size ? size : 1);
            if(buf) {
                size_t read = storage_file_read(file, buf, size);
                if(read == size) {
                    *out = buf;
                    *out_len = read;
                    ok = true;
                } else {
                    free(buf);
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    return ok;
}

static bool ck_load_plaintext_entries(CkApp* app) {
    uint8_t* raw = NULL;
    size_t len = 0;
    bool ok = false;

    if(ck_read_file(app, CK_PLAINTEXT_FILE, &raw, &len) && len < CK_MAX_VAULT_FILE) {
        char* buf = malloc(len + 1);
        if(buf) {
            memcpy(buf, raw, len);
            buf[len] = '\0';
            ck_parse_entries(app, buf);
            ck_secure_zero(buf, len);
            free(buf);
            ok = true;
        }
    }

    if(raw) {
        ck_secure_zero(raw, len);
        free(raw);
    }
    return ok;
}

static bool ck_serialize_entries(CkApp* app, uint8_t** out, size_t* out_len) {
    size_t cap = CK_MAX_ENTRIES * CK_LINE_MAX + 1;
    char* buf = malloc(cap);
    if(!buf) return false;

    size_t used = 0;
    buf[0] = '\0';
    for(uint8_t i = 0; i < app->entry_count; i++) {
        int len = snprintf(
            buf + used,
            cap - used,
            "%s\t%s\t%s\n",
            app->entries[i].account,
            app->entries[i].username,
            app->entries[i].password);
        if(len <= 0 || (size_t)len >= cap - used) {
            ck_secure_zero(buf, cap);
            free(buf);
            return false;
        }
        used += (size_t)len;
    }

    *out = (uint8_t*)buf;
    *out_len = used;
    return true;
}

static bool ck_load_encrypted_entries(CkApp* app, const char* pin) {
    uint8_t* file_buf = NULL;
    uint8_t* plain = NULL;
    size_t file_len = 0;
    bool ok = false;
    uint8_t key[CK_KEY_LEN];

    if(!ck_read_file(app, CK_VAULT_FILE, &file_buf, &file_len)) return false;
    if(file_len < CK_HDR_LEN || memcmp(file_buf, ck_vault_magic, CK_MAGIC_LEN) != 0) goto cleanup;

    const uint8_t* salt = file_buf + CK_MAGIC_LEN;
    const uint8_t* nonce = salt + CK_SALT_LEN;
    const uint8_t* tag = nonce + CK_NONCE_LEN;
    const uint8_t* cipher = tag + CK_TAG_LEN;
    size_t cipher_len = file_len - CK_HDR_LEN;

    plain = malloc(cipher_len + 1);
    if(!plain) goto cleanup;

    ck_derive_key_from_pin(pin, salt, key);
    FuriHalCryptoGCMState state = furi_hal_crypto_gcm_decrypt_and_verify(
        key, nonce, ck_vault_magic, CK_MAGIC_LEN, cipher, plain, cipher_len, tag);
    if(state != FuriHalCryptoGCMStateOk) goto cleanup;

    plain[cipher_len] = '\0';
    memcpy(app->vault_key, key, CK_KEY_LEN);
    memcpy(app->vault_salt, salt, CK_SALT_LEN);
    app->unlocked = true;
    ck_parse_entries(app, (char*)plain);
    ok = true;

cleanup:
    ck_secure_zero(key, sizeof(key));
    if(plain) {
        ck_secure_zero(plain, file_len > CK_HDR_LEN ? file_len - CK_HDR_LEN : 0);
        free(plain);
    }
    if(file_buf) {
        ck_secure_zero(file_buf, file_len);
        free(file_buf);
    }
    return ok;
}

static bool ck_save_entries(CkApp* app) {
    if(!app->unlocked) return false;

    uint8_t* plain = NULL;
    uint8_t* cipher = NULL;
    size_t plain_len = 0;
    uint8_t nonce[CK_NONCE_LEN];
    uint8_t tag[CK_TAG_LEN];
    bool ok = false;

    if(!ck_serialize_entries(app, &plain, &plain_len)) return false;
    cipher = malloc(plain_len ? plain_len : 1);
    if(!cipher) goto cleanup;

    furi_hal_random_fill_buf(nonce, CK_NONCE_LEN);
    FuriHalCryptoGCMState state = furi_hal_crypto_gcm_encrypt_and_tag(
        app->vault_key, nonce, ck_vault_magic, CK_MAGIC_LEN, plain, cipher, plain_len, tag);
    if(state != FuriHalCryptoGCMStateOk) goto cleanup;

    FuriString* path = ck_app_data_path(app, CK_VAULT_FILE);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = true;
        ok = ok && storage_file_write(file, ck_vault_magic, CK_MAGIC_LEN) == CK_MAGIC_LEN;
        ok = ok && storage_file_write(file, app->vault_salt, CK_SALT_LEN) == CK_SALT_LEN;
        ok = ok && storage_file_write(file, nonce, CK_NONCE_LEN) == CK_NONCE_LEN;
        ok = ok && storage_file_write(file, tag, CK_TAG_LEN) == CK_TAG_LEN;
        if(plain_len) ok = ok && storage_file_write(file, cipher, plain_len) == plain_len;
        if(ok) storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);

cleanup:
    if(plain) {
        ck_secure_zero(plain, plain_len);
        free(plain);
    }
    if(cipher) {
        ck_secure_zero(cipher, plain_len);
        free(cipher);
    }
    ck_secure_zero(nonce, sizeof(nonce));
    ck_secure_zero(tag, sizeof(tag));
    return ok;
}

static void ck_submenu_callback(void* context, uint32_t index) {
    CkApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, index);
}

static void ck_text_input_callback(void* context) {
    CkApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, CkEventTextDone);
}

static void ck_widget_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    CkApp* app = context;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventWidgetBack);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventWidgetInject);
    }
}

static void ck_dialog_callback(DialogExResult result, void* context) {
    CkApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventDialogRight);
    } else if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->dispatcher, CkEventDialogLeft);
    }
}

static bool ck_custom_event_callback(void* context, uint32_t event) {
    ck_handle_event(context, event);
    return true;
}

static bool ck_navigation_callback(void* context) {
    CkApp* app = context;
    if(app->dialog_purpose == CkDialogFido2Presence &&
       ck_fido2_service_presence_pending(app->fido2_service)) {
        ck_fido2_service_answer_presence(app->fido2_service, false);
        ck_stop_fido2(app);
        ck_show_main(app);
        return true;
    }
    if(app->security_key_view) {
        ck_stop_fido2(app);
        ck_show_main(app);
        return true;
    }
    if(!app->unlocked) {
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(app->current_view == CkViewMain && app->menu_mode == CkMenuModeMain) {
        view_dispatcher_stop(app->dispatcher);
    } else {
        ck_show_main(app);
    }
    return true;
}

static void ck_show_main(CkApp* app) {
    app->security_key_view = false;
    app->menu_mode = CkMenuModeMain;
    app->selected = -1;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "PāSSVΛŭLƬ");
    submenu_add_item(app->submenu, "+ Add New Password", CkEventAdd, ck_submenu_callback, app);
    submenu_add_item(app->submenu, "FIDO2 Security Key", CkEventSecurityKey, ck_submenu_callback, app);
    submenu_add_item(app->submenu, "About / ck42x.com", CkEventAbout, ck_submenu_callback, app);
    for(uint8_t i = 0; i < app->entry_count; i++) {
        submenu_add_item(
            app->submenu, app->entries[i].account, CkEventSavedBase + i, ck_submenu_callback, app);
    }
    app->current_view = CkViewMain;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewMain);
}

static void ck_show_generate_or_custom(CkApp* app) {
    app->menu_mode = CkMenuModeGenerateOrCustom;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Password Source");
    submenu_add_item(
        app->submenu, "Generate Password", CkEventChooseGenerate, ck_submenu_callback, app);
    submenu_add_item(app->submenu, "Enter Custom", CkEventChooseCustom, ck_submenu_callback, app);
    app->current_view = CkViewMain;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewMain);
}

static void ck_show_preset_menu(CkApp* app) {
    app->menu_mode = CkMenuModePreset;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Requirement Preset");
    submenu_add_item(
        app->submenu, "Memorable 16+ mix", CkEventPresetMemorable, ck_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Strict 16+ A/a/0/!", CkEventPresetStrict, ck_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Long 20+ passphrase", CkEventPresetLong, ck_submenu_callback, app);
    submenu_add_item(
        app->submenu, "No special char", CkEventPresetNoSymbol, ck_submenu_callback, app);
    app->current_view = CkViewMain;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewMain);
}

static void ck_show_text_input(CkApp* app, CkInputStage stage, const char* header, char* initial) {
    app->input_stage = stage;
    text_input_reset(app->text_input);
    memset(app->input, 0, sizeof(app->input));
    if(initial) ck_copy(app->input, sizeof(app->input), initial);
    text_input_set_header_text(app->text_input, header);
    text_input_set_minimum_length(
        app->text_input,
        (stage == CkInputSetPin || stage == CkInputUnlockPin) ? CK_PIN_MIN_LEN : 1);
    text_input_set_result_callback(
        app->text_input, ck_text_input_callback, app, app->input, sizeof(app->input), false);
    app->current_view = CkViewTextInput;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewTextInput);
}

static void ck_show_entry_widget(CkApp* app) {
    if(app->selected < 0 || app->selected >= app->entry_count) {
        ck_show_main(app);
        return;
    }
    CkVaultEntry* e = &app->entries[app->selected];
    char text[320];
    snprintf(
        text,
        sizeof(text),
        "\e#%s\n\e#User: %s\n\e*Pass: %s\e*\n\nRight = HID type password",
        e->account,
        e->username,
        e->password);

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, text);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Inject", ck_widget_button_callback, app);
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_show_save_dialog(CkApp* app) {
    char text[240];
    snprintf(
        text,
        sizeof(text),
        "Acct: %s\nUser: %s\nPass: %s",
        app->draft.account,
        app->draft.username,
        app->draft.password);
    app->dialog_purpose = CkDialogSave;
    dialog_ex_reset(app->dialog);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, ck_dialog_callback);
    dialog_ex_set_header(app->dialog, "Confirm Entry?", 64, 6, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, text, 4, 18, AlignLeft, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "No");
    dialog_ex_set_right_button_text(app->dialog, "Enter");
    app->current_view = CkViewDialog;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewDialog);
}

static void ck_show_inject_confirm(CkApp* app) {
    if(app->selected < 0 || app->selected >= app->entry_count) {
        ck_show_main(app);
        return;
    }
    app->dialog_purpose = CkDialogInjectConfirm;
    dialog_ex_reset(app->dialog);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, ck_dialog_callback);
    dialog_ex_set_header(app->dialog, "HID Inject?", 64, 8, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog,
        "Focus the password field.\nRight types password only.\nLeft cancels.",
        4,
        20,
        AlignLeft,
        AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Cancel");
    dialog_ex_set_right_button_text(app->dialog, "Type");
    app->current_view = CkViewDialog;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewDialog);
}

static void ck_show_status(CkApp* app, const char* header, const char* message) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, header);
    widget_add_text_box_element(
        app->widget, 4, 20, 120, 30, AlignCenter, AlignCenter, message, false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_show_about(CkApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "CK42X PassVault");
    widget_add_text_box_element(
        app->widget,
        4,
        20,
        120,
        32,
        AlignCenter,
        AlignCenter,
        "Password vault tool\nBuild. Code. Transmute.\nck42x.com",
        false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_show_security_key(CkApp* app) {
    if(!app->fido2_service) {
        app->fido2_service =
            ck_fido2_service_alloc(app->storage, app->vault_key, app->dispatcher);
    }
    if(!app->fido2_service || !ck_fido2_service_start(app->fido2_service)) {
        ck_show_status(app, "FIDO2 Failed", "Could not start\nUSB security key.");
        return;
    }
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "CK42X FIDO2");
    widget_add_text_box_element(
        app->widget,
        4,
        20,
        120,
        32,
        AlignCenter,
        AlignCenter,
        "Ready / waiting\nExperimental runtime\nHardware proof pending",
        false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", ck_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Stop", ck_widget_button_callback, app);
    app->security_key_view = true;
    app->current_view = CkViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewWidget);
}

static void ck_show_fido2_presence(CkApp* app) {
    if(!ck_fido2_service_presence_pending(app->fido2_service)) return;
    app->dialog_purpose = CkDialogFido2Presence;
    dialog_ex_reset(app->dialog);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, ck_dialog_callback);
    dialog_ex_set_header(app->dialog, "FIDO2 Request", 64, 7, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog,
        "Approve presence?\nCheck the requesting site.\nTimes out in 30 seconds.",
        4,
        20,
        AlignLeft,
        AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Deny");
    dialog_ex_set_right_button_text(app->dialog, "Approve");
    app->security_key_view = true;
    app->current_view = CkViewDialog;
    view_dispatcher_switch_to_view(app->dispatcher, CkViewDialog);
}

static void ck_stop_fido2(CkApp* app) {
    if(!app->fido2_service) return;
    if(ck_fido2_service_presence_pending(app->fido2_service))
        ck_fido2_service_answer_presence(app->fido2_service, false);
    ck_fido2_service_stop(app->fido2_service);
    app->security_key_view = false;
}

static void ck_hid_type_string(const char* text) {
    for(const char* p = text; *p; p++) {
        uint16_t key = HID_ASCII_TO_KEY(*p);
        if(key == HID_KEYBOARD_NONE) continue;
        furi_hal_hid_kb_press(key);
        furi_delay_ms(18);
        furi_hal_hid_kb_release_all();
        furi_delay_ms(12);
    }
}

static bool ck_inject_selected(CkApp* app) {
    if(app->selected < 0 || app->selected >= app->entry_count) return false;
    app->previous_usb = furi_hal_usb_get_config();
    if(app->previous_usb != &usb_hid) {
        if(!furi_hal_usb_set_config(&usb_hid, NULL)) return false;
        furi_delay_ms(800);
    }

    ck_hid_type_string(app->entries[app->selected].password);
    furi_hal_hid_kb_release_all();
    furi_delay_ms(100);

    if(app->previous_usb && app->previous_usb != &usb_hid) {
        furi_hal_usb_set_config(app->previous_usb, NULL);
    }
    return true;
}

static void ck_begin_auth(CkApp* app) {
    app->unlocked = false;
    app->entry_count = 0;
    if(ck_file_exists(app, CK_VAULT_FILE)) {
        ck_show_text_input(app, CkInputUnlockPin, "Enter Master PIN", NULL);
    } else {
        ck_show_text_input(app, CkInputSetPin, "Set Master PIN", NULL);
    }
}

static void ck_handle_set_pin(CkApp* app) {
    if(strlen(app->input) < CK_PIN_MIN_LEN) {
        ck_show_text_input(app, CkInputSetPin, "PIN: 4+ chars", NULL);
        return;
    }

    bool had_plaintext = ck_file_exists(app, CK_PLAINTEXT_FILE);
    furi_hal_random_fill_buf(app->vault_salt, CK_SALT_LEN);
    ck_derive_key_from_pin(app->input, app->vault_salt, app->vault_key);
    ck_secure_zero(app->input, sizeof(app->input));
    app->unlocked = true;
    app->entry_count = 0;

    if(had_plaintext && !ck_load_plaintext_entries(app)) {
        app->unlocked = false;
        ck_secure_zero(app->vault_key, sizeof(app->vault_key));
        ck_show_status(app, "Migration Failed", "Plaintext vault\nwas not imported.");
        return;
    }

    if(ck_save_entries(app)) {
        if(had_plaintext) ck_remove_file(app, CK_PLAINTEXT_FILE);
        ck_show_main(app);
    } else {
        app->unlocked = false;
        ck_secure_zero(app->vault_key, sizeof(app->vault_key));
        ck_show_status(app, "Setup Failed", "Could not write\nencrypted vault.");
    }
}

static void ck_handle_unlock_pin(CkApp* app) {
    if(ck_load_encrypted_entries(app, app->input)) {
        ck_secure_zero(app->input, sizeof(app->input));
        ck_show_main(app);
    } else {
        ck_secure_zero(app->input, sizeof(app->input));
        ck_show_text_input(app, CkInputUnlockPin, "PIN Incorrect - Retry", NULL);
    }
}

static void ck_handle_text_done(CkApp* app) {
    if(app->input_stage == CkInputSetPin) {
        ck_handle_set_pin(app);
        return;
    } else if(app->input_stage == CkInputUnlockPin) {
        ck_handle_unlock_pin(app);
        return;
    }

    ck_sanitize(app->input);
    if(app->input_stage == CkInputAccount) {
        memset(&app->draft, 0, sizeof(app->draft));
        ck_copy(app->draft.account, sizeof(app->draft.account), app->input);
        ck_show_text_input(app, CkInputUsername, "Username", NULL);
    } else if(app->input_stage == CkInputUsername) {
        ck_copy(app->draft.username, sizeof(app->draft.username), app->input);
        ck_show_generate_or_custom(app);
    } else if(app->input_stage == CkInputCustomPassword) {
        ck_copy(app->draft.password, sizeof(app->draft.password), app->input);
        ck_show_save_dialog(app);
    }
}

static void ck_save_draft(CkApp* app) {
    if(app->entry_count >= CK_MAX_ENTRIES) {
        ck_show_status(app, "Vault Full", "Max entries reached.");
        return;
    }
    uint8_t saved_index = app->entry_count;
    app->entries[app->entry_count++] = app->draft;
    bool ok = ck_save_entries(app);
    if(ok) {
        app->selected = saved_index;
        ck_show_entry_widget(app);
    } else {
        ck_show_status(app, "Save Failed", "SD/app data write failed.");
    }
}

static void ck_handle_event(CkApp* app, uint32_t event) {
    if(!app->unlocked && event != CkEventTextDone && event != CkEventWidgetBack) return;

    if(event == CkEventAdd) {
        ck_show_text_input(app, CkInputAccount, "Name", NULL);
    } else if(event == CkEventSecurityKey) {
        ck_show_security_key(app);
    } else if(event == CkEventAbout) {
        ck_show_about(app);
    } else if(event >= CkEventSavedBase && event < CkEventSavedBase + CK_MAX_ENTRIES) {
        uint32_t idx = event - CkEventSavedBase;
        if(idx < app->entry_count) {
            app->selected = idx;
            ck_show_entry_widget(app);
        }
    } else if(event == CkEventTextDone) {
        ck_handle_text_done(app);
    } else if(event == CkEventChooseGenerate) {
        ck_show_preset_menu(app);
    } else if(event == CkEventChooseCustom) {
        ck_show_text_input(app, CkInputCustomPassword, "Custom Password", NULL);
    } else if(
        event == CkEventPresetMemorable || event == CkEventPresetStrict ||
        event == CkEventPresetLong || event == CkEventPresetNoSymbol) {
        CkPreset preset = CkPresetMemorable;
        if(event == CkEventPresetStrict) preset = CkPresetStrict;
        if(event == CkEventPresetLong) preset = CkPresetLong;
        if(event == CkEventPresetNoSymbol) preset = CkPresetNoSymbol;
        ck_generate_password(preset, app->draft.password, sizeof(app->draft.password));
        ck_make_password_unique(app, app->draft.password, sizeof(app->draft.password));
        ck_show_save_dialog(app);
    } else if(event == CkEventWidgetBack) {
        if(app->security_key_view) {
            ck_stop_fido2(app);
            ck_show_main(app);
        } else if(app->unlocked)
            ck_show_main(app);
        else
            ck_begin_auth(app);
    } else if(event == CkEventWidgetInject) {
        if(app->security_key_view) {
            ck_stop_fido2(app);
            ck_show_main(app);
        } else {
            ck_show_inject_confirm(app);
        }
    } else if(event == CkFido2ServiceEventPresence) {
        ck_show_fido2_presence(app);
    } else if(event == CkFido2ServiceEventPresenceDone) {
        if(app->security_key_view && app->current_view == CkViewDialog &&
           app->dialog_purpose == CkDialogFido2Presence)
            ck_show_security_key(app);
    } else if(event == CkEventDialogLeft) {
        if(app->dialog_purpose == CkDialogFido2Presence) {
            ck_fido2_service_answer_presence(app->fido2_service, false);
            ck_show_security_key(app);
        } else if(app->dialog_purpose == CkDialogInjectConfirm)
            ck_show_entry_widget(app);
        else
            ck_show_main(app);
    } else if(event == CkEventDialogRight) {
        if(app->dialog_purpose == CkDialogSave) {
            ck_save_draft(app);
        } else if(app->dialog_purpose == CkDialogInjectConfirm) {
            bool ok = ck_inject_selected(app);
            if(ok)
                ck_show_entry_widget(app);
            else
                ck_show_status(app, "HID Failed", "Could not switch USB HID.");
        } else if(app->dialog_purpose == CkDialogFido2Presence) {
            ck_fido2_service_answer_presence(app->fido2_service, true);
            ck_show_security_key(app);
        }
    }
}

static CkApp* ck_app_alloc(void) {
    CkApp* app = malloc(sizeof(CkApp));
    furi_assert(app);
    memset(app, 0, sizeof(CkApp));
    app->selected = -1;

    app->storage = furi_record_open(RECORD_STORAGE);

    app->dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->widget = widget_alloc();
    app->dialog = dialog_ex_alloc();

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, ck_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, ck_navigation_callback);

    view_dispatcher_add_view(app->dispatcher, CkViewMain, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->dispatcher, CkViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->dispatcher, CkViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->dispatcher, CkViewDialog, dialog_ex_get_view(app->dialog));

    return app;
}

static void ck_app_free(CkApp* app) {
    if(!app) return;
    ck_fido2_service_free(app->fido2_service);
    app->fido2_service = NULL;
    view_dispatcher_remove_view(app->dispatcher, CkViewMain);
    view_dispatcher_remove_view(app->dispatcher, CkViewTextInput);
    view_dispatcher_remove_view(app->dispatcher, CkViewWidget);
    view_dispatcher_remove_view(app->dispatcher, CkViewDialog);
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_STORAGE);
    ck_secure_zero(app->vault_key, sizeof(app->vault_key));
    ck_secure_zero(app->vault_salt, sizeof(app->vault_salt));
    ck_secure_zero(app->input, sizeof(app->input));
    free(app);
}

int32_t ck42x_passvault_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(CK_TAG, "Starting CK42X PassVault");

    CkApp* app = ck_app_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    ck_begin_auth(app);
    view_dispatcher_run(app->dispatcher);

    ck_stop_fido2(app);
    furi_record_close(RECORD_GUI);
    ck_app_free(app);
    FURI_LOG_I(CK_TAG, "Stopped CK42X PassVault");
    return 0;
}
