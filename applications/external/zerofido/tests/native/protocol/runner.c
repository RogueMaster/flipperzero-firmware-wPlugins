#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZF_AUTO_ACCEPT_REQUESTS 1
#define ZF_DEV_FIDO2_1 1
#ifndef ZF_PACKED_ATTESTATION
#define ZF_PACKED_ATTESTATION 1
#endif

#include "furi.h"
#include "flipper_format/flipper_format.h"
#include "mbedtls/ecp.h"
#include "store/record_format.h"
#include "store/recovery.h"
#include "zerofido_attestation.h"
#include "zerofido_app_i.h"
#include "zerofido_cbor.h"
#include "zerofido_crypto.h"
#include "ctap/parse.h"
#include "ctap/policy.h"
#include "ctap/response.h"
#include "store/internal.h"
#include "pin/store/internal.h"
#include "store/record_format_internal.h"

/*
 * Host-native protocol regression harness. This file provides fake Flipper
 * platform services, includes the production C modules directly, and exercises
 * CTAP/PIN/store behavior without device hardware.
 */

#define FURI_PACKED __attribute__((packed))
static void test_furi_log_i(const char *tag, const char *fmt, ...);
#define FURI_LOG_I test_furi_log_i
#define FURI_LOG_E(tag, fmt, ...) ((void)0)
#define FURI_LOG_W(tag, fmt, ...) ((void)0)
#define FURI_LOG_D(tag, fmt, ...) ((void)0)
#define RECORD_GUI "gui"
#define RECORD_NOTIFICATION "notification"
#define RECORD_STORAGE "storage"
#define EXT_PATH(path) path
#define __REV(value) __builtin_bswap32(value)
#define furi_assert(expr)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "FAIL: assertion failed: %s\n", #expr);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

struct Storage {
    int unused;
};

struct File {
    char path[160];
    FS_AccessMode access_mode;
    size_t offset;
    bool open;
};

struct FileInfo {
    int unused;
};

struct FlipperFormat {
    int unused;
};

struct FuriString {
    char value[64];
};

struct FuriThread {
    FuriThreadCallback callback;
    void *context;
    size_t stack_size;
    bool started;
    bool joined;
};

static uint8_t g_key_agreement_seed = 1;
static bool g_fail_crypto_encrypt = false;
static bool g_fail_crypto_decrypt = false;
static size_t g_fail_crypto_decrypt_after = SIZE_MAX;
static bool g_fail_crypto_enclave_load_key = false;
static bool g_crypto_verify_hash_result = true;
static uint8_t g_pin_file_data[128];
static size_t g_pin_file_size = 0;
static bool g_pin_file_exists = false;
static uint8_t g_pin_temp_data[128];
static size_t g_pin_temp_size = 0;
static bool g_pin_temp_exists = false;
static bool g_remove_attempted_pin_v2_file = false;
static bool g_remove_attempted_pin_v2_temp = false;
static const char *g_storage_fail_rename_match = NULL;
static const char *g_storage_fail_remove_match = NULL;
static const char *g_storage_fail_copy_match = NULL;
static const char *g_storage_fail_write_match = NULL;
static const char *g_storage_fail_mkdir_match = NULL;
static size_t g_approval_request_count = 0;
static bool g_approval_request_ok = true;
static bool g_approval_result = true;
static ZfApprovalState g_approval_state = ZfApprovalApproved;
static bool g_attestation_ensure_ready = true;
static size_t g_selection_request_count = 0;
static bool g_selection_request_ok = true;
static size_t g_selection_index = 0;
static size_t g_last_selection_match_count = 0;
static uint32_t g_fake_tick = 1000;
static uint8_t g_transport_poll_status = ZF_CTAP_SUCCESS;
static size_t g_mutex_depth = 0;
static bool g_transport_poll_requires_unlocked = false;
static bool g_thread_alloc_fail = false;
static size_t g_thread_start_count = 0;
static size_t g_thread_join_count = 0;
static size_t g_thread_free_count = 0;
static bool g_u2f_adapter_init_ok = true;
static size_t g_u2f_adapter_init_count = 0;
static size_t g_u2f_adapter_deinit_count = 0;
static size_t g_storage_file_alloc_index = 0;
static size_t g_storage_counter_rename_count = 0;
static bool g_storage_root_exists = true;
static bool g_storage_app_data_exists = true;
static char g_last_status_text[160];
static size_t g_status_update_count = 0;
static char g_last_log_text[192];
static char g_log_text[4096];
static size_t g_log_i_count = 0;
#define TEST_STORAGE_MAX_FILE_SIZE 800
typedef struct {
    bool in_use;
    char path[128];
    uint8_t data[TEST_STORAGE_MAX_FILE_SIZE];
    size_t size;
    bool exists;
} TestStorageFile;
static TestStorageFile g_storage_files[8];
static const char *k_repeated_credential_file_name =
    "1010101010101010101010101010101010101010101010101010101010101010";

static void test_furi_log_i(const char *tag, const char *fmt, ...) {
    char line[192];
    size_t used = 0U;
    va_list args;

    UNUSED(tag);
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    snprintf(g_last_log_text, sizeof(g_last_log_text), "%s", line);
    used = strlen(g_log_text);
    if (used < sizeof(g_log_text) - 1U) {
        snprintf(&g_log_text[used], sizeof(g_log_text) - used, "%s%s", used ? "\n" : "", line);
    }
    g_log_i_count++;
}

static bool test_log_contains(const char *needle) {
    return needle && strstr(g_log_text, needle) != NULL;
}

static bool test_storage_copy_basename(const char *path, char *name, size_t name_size) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;

    if (strlen(base) >= name_size) {
        return false;
    }

    strncpy(name, base, name_size - 1);
    name[name_size - 1] = '\0';
    return true;
}

static TestStorageFile *test_storage_file_slot(const char *path, bool create) {
    size_t free_index = SIZE_MAX;

    for (size_t i = 0; i < (sizeof(g_storage_files) / sizeof(g_storage_files[0])); ++i) {
        if (g_storage_files[i].in_use && strcmp(g_storage_files[i].path, path) == 0) {
            return &g_storage_files[i];
        }
        if (create && free_index == SIZE_MAX && !g_storage_files[i].in_use) {
            free_index = i;
        }
    }

    if (!create || free_index == SIZE_MAX) {
        return NULL;
    }

    g_storage_files[free_index].in_use = true;
    strncpy(g_storage_files[free_index].path, path, sizeof(g_storage_files[free_index].path) - 1);
    g_storage_files[free_index].path[sizeof(g_storage_files[free_index].path) - 1] = '\0';
    g_storage_files[free_index].size = 0;
    g_storage_files[free_index].exists = false;
    memset(g_storage_files[free_index].data, 0, sizeof(g_storage_files[free_index].data));
    return &g_storage_files[free_index];
}

static uint8_t *test_storage_select_data(const char *path, size_t **size_out, bool **exists_out) {
    TestStorageFile *slot = NULL;

    if (strstr(path, "client_pin") != NULL && strstr(path, ".bak") == NULL &&
        strstr(path, ".tmp") != NULL) {
        *size_out = &g_pin_temp_size;
        *exists_out = &g_pin_temp_exists;
        return g_pin_temp_data;
    }
    if (strstr(path, "client_pin") != NULL && strstr(path, ".bak") == NULL &&
        strstr(path, ".bin") != NULL) {
        *size_out = &g_pin_file_size;
        *exists_out = &g_pin_file_exists;
        return g_pin_file_data;
    }

    slot = test_storage_file_slot(path, true);
    if (!slot) {
        *size_out = NULL;
        *exists_out = NULL;
        return NULL;
    }

    *size_out = &slot->size;
    *exists_out = &slot->exists;
    return slot->data;
}

static void test_storage_reset(void) {
    g_fail_crypto_encrypt = false;
    g_fail_crypto_decrypt = false;
    g_fail_crypto_decrypt_after = SIZE_MAX;
    g_fail_crypto_enclave_load_key = false;
    g_crypto_verify_hash_result = true;
    memset(g_pin_file_data, 0, sizeof(g_pin_file_data));
    g_pin_file_size = 0;
    g_pin_file_exists = false;
    memset(g_pin_temp_data, 0, sizeof(g_pin_temp_data));
    g_pin_temp_size = 0;
    g_pin_temp_exists = false;
    g_remove_attempted_pin_v2_file = false;
    g_remove_attempted_pin_v2_temp = false;
    g_storage_fail_rename_match = NULL;
    g_storage_fail_remove_match = NULL;
    g_storage_fail_copy_match = NULL;
    g_storage_fail_write_match = NULL;
    g_storage_fail_mkdir_match = NULL;
    g_approval_request_count = 0;
    g_approval_request_ok = true;
    g_approval_result = true;
    g_approval_state = ZfApprovalApproved;
    g_attestation_ensure_ready = true;
    g_selection_request_count = 0;
    g_selection_request_ok = true;
    g_selection_index = 0;
    g_last_selection_match_count = 0;
    g_fake_tick = 1000;
    g_transport_poll_status = ZF_CTAP_SUCCESS;
    g_mutex_depth = 0;
    g_transport_poll_requires_unlocked = false;
    g_thread_alloc_fail = false;
    g_thread_start_count = 0;
    g_thread_join_count = 0;
    g_thread_free_count = 0;
    g_u2f_adapter_init_ok = true;
    g_u2f_adapter_init_count = 0;
    g_u2f_adapter_deinit_count = 0;
    g_storage_file_alloc_index = 0;
    g_storage_counter_rename_count = 0;
    g_storage_root_exists = true;
    g_storage_app_data_exists = true;
    memset(g_last_status_text, 0, sizeof(g_last_status_text));
    g_status_update_count = 0;
    memset(g_last_log_text, 0, sizeof(g_last_log_text));
    memset(g_log_text, 0, sizeof(g_log_text));
    g_log_i_count = 0;
    memset(g_storage_files, 0, sizeof(g_storage_files));
}

bool file_info_is_dir(const FileInfo *info) {
    UNUSED(info);
    return false;
}

FS_Error storage_common_remove(Storage *storage, const char *path) {
    size_t *size = NULL;
    bool *exists = NULL;

    UNUSED(storage);
    if (strcmp(path, ZF_APP_DATA_DIR "/client_pin_v2.bin") == 0) {
        g_remove_attempted_pin_v2_file = true;
    } else if (strcmp(path, ZF_APP_DATA_DIR "/client_pin_v2.tmp") == 0) {
        g_remove_attempted_pin_v2_temp = true;
    }
    if (g_storage_fail_remove_match && strstr(path, g_storage_fail_remove_match) != NULL) {
        return FSE_NOT_EXIST;
    }
    if (test_storage_select_data(path, &size, &exists) == NULL || !*exists) {
        return FSE_NOT_EXIST;
    }

    *size = 0;
    *exists = false;
    return FSE_OK;
}

FS_Error storage_common_copy(Storage *storage, const char *old_path, const char *new_path) {
    size_t *old_size = NULL;
    size_t *new_size = NULL;
    bool *old_exists = NULL;
    bool *new_exists = NULL;
    uint8_t *old_data = NULL;
    uint8_t *new_data = NULL;

    UNUSED(storage);
    if (g_storage_fail_copy_match && (strstr(old_path, g_storage_fail_copy_match) != NULL ||
                                      strstr(new_path, g_storage_fail_copy_match) != NULL)) {
        return FSE_NOT_EXIST;
    }
    old_data = test_storage_select_data(old_path, &old_size, &old_exists);
    new_data = test_storage_select_data(new_path, &new_size, &new_exists);
    if (old_data == NULL || new_data == NULL || !*old_exists) {
        return FSE_NOT_EXIST;
    }
    memcpy(new_data, old_data, *old_size);
    *new_size = *old_size;
    *new_exists = true;
    return FSE_OK;
}

FS_Error storage_common_rename(Storage *storage, const char *old_path, const char *new_path) {
    FS_Error copy_result = FSE_NOT_EXIST;
    size_t *old_size = NULL;
    bool *old_exists = NULL;
    uint8_t *old_data = NULL;

    if (g_storage_fail_rename_match && (strstr(old_path, g_storage_fail_rename_match) != NULL ||
                                        strstr(new_path, g_storage_fail_rename_match) != NULL)) {
        return FSE_NOT_EXIST;
    }
    old_data = test_storage_select_data(old_path, &old_size, &old_exists);
    if (old_data == NULL || !*old_exists) {
        return FSE_NOT_EXIST;
    }

    if (storage_file_exists(storage, new_path)) {
        storage_common_remove(storage, new_path);
    }
    copy_result = storage_common_copy(storage, old_path, new_path);
    if (copy_result != FSE_OK) {
        return copy_result;
    }
    if (strstr(new_path, ".counter") != NULL) {
        g_storage_counter_rename_count++;
    }

    memset(old_data, 0, *old_size);
    *old_size = 0;
    *old_exists = false;
    return FSE_OK;
}

bool storage_dir_exists(Storage *storage, const char *path) {
    UNUSED(storage);
    if (strcmp(path, ZF_APP_DATA_ROOT) == 0) {
        return g_storage_root_exists;
    }
    if (strcmp(path, ZF_APP_DATA_DIR) == 0) {
        return g_storage_app_data_exists;
    }
    return true;
}

bool storage_simply_mkdir(Storage *storage, const char *path) {
    UNUSED(storage);
    if (g_storage_fail_mkdir_match && strstr(path, g_storage_fail_mkdir_match) != NULL) {
        return false;
    }
    if (strcmp(path, ZF_APP_DATA_ROOT) == 0) {
        g_storage_root_exists = true;
    } else if (strcmp(path, ZF_APP_DATA_DIR) == 0) {
        g_storage_app_data_exists = true;
    }
    return true;
}

bool storage_file_exists(Storage *storage, const char *path) {
    TestStorageFile *slot = NULL;

    UNUSED(storage);
    if (strstr(path, "client_pin") != NULL && strstr(path, ".bak") == NULL &&
        strstr(path, ".tmp") != NULL) {
        return g_pin_temp_exists;
    }
    if (strstr(path, "client_pin") != NULL && strstr(path, ".bak") == NULL &&
        strstr(path, ".bin") != NULL) {
        return g_pin_file_exists;
    }
    slot = test_storage_file_slot(path, false);
    return slot && slot->exists;
}

bool storage_dir_open(File *file, const char *path) {
    if (!storage_dir_exists(NULL, path)) {
        return false;
    }
    memset(file, 0, sizeof(*file));
    strncpy(file->path, path, sizeof(file->path) - 1);
    file->path[sizeof(file->path) - 1] = '\0';
    file->open = true;
    file->offset = 0;
    return true;
}

bool storage_dir_read(File *file, FileInfo *info, char *name, size_t name_size) {
    size_t prefix_len = 0;

    UNUSED(info);
    if (!file || !file->open) {
        return false;
    }

    prefix_len = strlen(file->path);
    for (; file->offset < (sizeof(g_storage_files) / sizeof(g_storage_files[0])); ++file->offset) {
        const TestStorageFile *slot = &g_storage_files[file->offset];
        if (!slot->in_use || !slot->exists) {
            continue;
        }
        if (strncmp(slot->path, file->path, prefix_len) != 0 || slot->path[prefix_len] != '/') {
            continue;
        }
        if (!test_storage_copy_basename(slot->path, name, name_size)) {
            return false;
        }
        file->offset++;
        return true;
    }

    return false;
}

void storage_dir_close(File *file) {
    UNUSED(file);
}

File *storage_file_alloc(Storage *storage) {
    static File files[4];
    File *file = NULL;

    UNUSED(storage);
    file = &files[g_storage_file_alloc_index % (sizeof(files) / sizeof(files[0]))];
    g_storage_file_alloc_index++;
    memset(file, 0, sizeof(*file));
    return file;
}

void storage_file_free(File *file) {
    UNUSED(file);
}

bool storage_file_open(File *file, const char *path, FS_AccessMode access_mode,
                       FS_OpenMode open_mode) {
    size_t *size = NULL;
    bool *exists = NULL;

    if (strncmp(path, ZF_APP_DATA_DIR "/", strlen(ZF_APP_DATA_DIR) + 1) == 0 &&
        !g_storage_app_data_exists) {
        return false;
    }
    if (test_storage_select_data(path, &size, &exists) == NULL) {
        return false;
    }
    if (access_mode == FSAM_READ && (!*exists || open_mode != FSOM_OPEN_EXISTING)) {
        return false;
    }
    if (access_mode == FSAM_WRITE && open_mode == FSOM_CREATE_ALWAYS) {
        *size = 0;
        *exists = true;
    }
    if (access_mode == FSAM_WRITE && open_mode == FSOM_OPEN_APPEND) {
        *exists = true;
    }

    memset(file, 0, sizeof(*file));
    strncpy(file->path, path, sizeof(file->path) - 1);
    file->path[sizeof(file->path) - 1] = '\0';
    file->access_mode = access_mode;
    file->offset = (access_mode == FSAM_WRITE && open_mode == FSOM_OPEN_APPEND) ? *size : 0U;
    file->open = true;
    return true;
}

size_t storage_file_size(File *file) {
    size_t *size = NULL;
    bool *exists = NULL;

    if (!file->open || test_storage_select_data(file->path, &size, &exists) == NULL || !*exists) {
        return 0;
    }
    return *size;
}

size_t storage_file_read(File *file, void *buffer, size_t size) {
    size_t *stored_size = NULL;
    bool *exists = NULL;
    uint8_t *data = NULL;
    size_t remaining = 0;

    if (!file->open || file->access_mode != FSAM_READ) {
        return 0;
    }
    data = test_storage_select_data(file->path, &stored_size, &exists);
    if (data == NULL || !*exists || file->offset >= *stored_size) {
        return 0;
    }

    remaining = *stored_size - file->offset;
    if (size > remaining) {
        size = remaining;
    }
    memcpy(buffer, data + file->offset, size);
    file->offset += size;
    return size;
}

size_t storage_file_write(File *file, const void *buffer, size_t size) {
    size_t *stored_size = NULL;
    bool *exists = NULL;
    uint8_t *data = NULL;

    if (!file->open || file->access_mode != FSAM_WRITE) {
        return 0;
    }
    if (g_storage_fail_write_match && strstr(file->path, g_storage_fail_write_match) != NULL) {
        return 0;
    }
    data = test_storage_select_data(file->path, &stored_size, &exists);
    if (data == NULL || (file->offset + size) > TEST_STORAGE_MAX_FILE_SIZE) {
        return 0;
    }

    memcpy(data + file->offset, buffer, size);
    file->offset += size;
    if (file->offset > *stored_size) {
        *stored_size = file->offset;
    }
    *exists = true;
    return size;
}

void storage_file_close(File *file) {
    file->open = false;
}

void *furi_record_open(const char *name) {
    static Storage storage;

    UNUSED(name);
    return &storage;
}

void furi_record_close(const char *name) {
    UNUSED(name);
}

uint32_t furi_hal_rtc_get_timestamp(void) {
    return 1234;
}

void furi_hal_random_fill_buf(void *buffer, size_t size) {
    memset(buffer, 0xA5, size);
}

bool furi_hal_crypto_enclave_ensure_key(uint8_t slot) {
    UNUSED(slot);
    return true;
}

bool furi_hal_crypto_enclave_load_key(uint8_t slot, const uint8_t *iv) {
    UNUSED(slot);
    UNUSED(iv);
    if (g_fail_crypto_enclave_load_key) {
        return false;
    }
    return true;
}

void furi_hal_crypto_enclave_unload_key(uint8_t slot) {
    UNUSED(slot);
}

bool furi_hal_crypto_load_key(const uint8_t *key, const uint8_t *iv) {
    UNUSED(key);
    UNUSED(iv);
    if (g_fail_crypto_enclave_load_key) {
        return false;
    }
    return true;
}

bool furi_hal_crypto_unload_key(void) {
    return true;
}

bool furi_hal_crypto_encrypt(const uint8_t *input, uint8_t *output, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        output[i] = input[i] ^ 0xA5U;
    }
    return true;
}

bool furi_hal_crypto_decrypt(const uint8_t *input, uint8_t *output, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        output[i] = input[i] ^ 0xA5U;
    }
    return true;
}

static const uint8_t k_test_aaguid[ZF_AAGUID_LEN] = {
    0xB5, 0x1A, 0x97, 0x6A, 0x0B, 0x02, 0x40, 0xAA, 0x9D, 0x8A, 0x36, 0xC8, 0xB9, 0x1B, 0xBD, 0x1A,
};

const uint8_t *zf_attestation_get_aaguid(void) {
    return k_test_aaguid;
}

const char *zf_attestation_get_aaguid_string(void) {
    return "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a";
}

bool zf_attestation_ensure_ready(void) {
    return g_attestation_ensure_ready;
}

bool zf_attestation_get_leaf_cert_der_len(size_t *out_len) {
    static const uint8_t cert[] = {0x30, 0x82, 0x00, 0x00};
    if (!out_len) {
        return false;
    }
    *out_len = sizeof(cert);
    return true;
}

bool zf_attestation_load_leaf_cert_der(uint8_t *out, size_t out_capacity, size_t *out_len) {
    static const uint8_t cert[] = {0x30, 0x82, 0x00, 0x00};
    if (!out || !out_len || out_capacity < sizeof(cert)) {
        return false;
    }
    memcpy(out, cert, sizeof(cert));
    *out_len = sizeof(cert);
    return true;
}

bool zf_attestation_sign_input(const uint8_t *input, size_t input_len, uint8_t *out,
                               size_t out_capacity, size_t *out_len) {
    UNUSED(input);
    UNUSED(input_len);
    UNUSED(out_capacity);
    out[0] = 0xAA;
    out[1] = 0x55;
    *out_len = 2;
    return true;
}

bool zf_attestation_sign_parts(const uint8_t *first, size_t first_len, const uint8_t *second,
                               size_t second_len, uint8_t *out, size_t out_capacity,
                               size_t *out_len) {
    UNUSED(first);
    UNUSED(first_len);
    UNUSED(second);
    UNUSED(second_len);
    UNUSED(out_capacity);
    out[0] = 0xAA;
    out[1] = 0x55;
    *out_len = 2;
    return true;
}

bool zf_attestation_validate_consistency(void) {
    return true;
}

void zf_attestation_reset_consistency_cache(void) {}

bool zf_crypto_ensure_store_key(void) {
    return true;
}

void zf_crypto_secure_zero(void *data, size_t size) {
    memset(data, 0, size);
}

void zf_crypto_sha256(const uint8_t *data, size_t size, uint8_t out[32]) {
    memset(out, 0, 32);
    for (size_t i = 0; i < size; ++i) {
        out[i % 32] ^= data[i];
    }
}

void zf_crypto_sha256_concat(const uint8_t *first, size_t first_size, const uint8_t *second,
                             size_t second_size, uint8_t out[32]) {
    zf_crypto_sha256(first, first_size, out);
    for (size_t i = 0; i < second_size; ++i) {
        out[i % 32] ^= second[i];
    }
}

bool zf_crypto_hmac_sha256_parts_with_scratch(ZfHmacSha256Scratch *scratch, const uint8_t *key,
                                              size_t key_len, const uint8_t *first,
                                              size_t first_size, const uint8_t *second,
                                              size_t second_size, uint8_t out[32]) {
    UNUSED(scratch);
    if (!key || !out || (first_size > 0U && !first) || (second_size > 0U && !second)) {
        return false;
    }

    zf_crypto_sha256(key, key_len, out);
    for (size_t i = 0; i < first_size; ++i) {
        out[i % 32] ^= first[i];
    }
    for (size_t i = 0; i < second_size; ++i) {
        out[i % 32] ^= second[i];
    }
    return true;
}

bool zf_crypto_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t size,
                           uint8_t out[32]) {
    ZfHmacSha256Scratch scratch;

    return zf_crypto_hmac_sha256_parts_with_scratch(&scratch, key, key_len, data, size, NULL, 0,
                                                    out);
}

bool zf_crypto_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                           const uint8_t *info, size_t info_len, uint8_t out[32]) {
    UNUSED(salt);
    UNUSED(salt_len);
    uint8_t hmac[32];
    if (!zf_crypto_hmac_sha256(ikm, ikm_len, info, info_len, hmac)) {
        return false;
    }
    memcpy(out, hmac, 32);
    return true;
}

bool zf_crypto_aes256_cbc_encrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                  uint8_t *output, size_t size) {
    UNUSED(key);
    UNUSED(iv);
    if (g_fail_crypto_encrypt) {
        return false;
    }
    memcpy(output, input, size);
    return true;
}

bool zf_crypto_aes256_cbc_decrypt(const uint8_t key[32], const uint8_t iv[16], const uint8_t *input,
                                  uint8_t *output, size_t size) {
    UNUSED(key);
    UNUSED(iv);
    if (g_fail_crypto_decrypt) {
        return false;
    }
    if (g_fail_crypto_decrypt_after != SIZE_MAX) {
        if (g_fail_crypto_decrypt_after == 0) {
            return false;
        }
        g_fail_crypto_decrypt_after--;
    }
    memcpy(output, input, size);
    return true;
}

bool zf_crypto_aes256_cbc_zero_iv_encrypt(const uint8_t key[32], const uint8_t *input,
                                          uint8_t *output, size_t size) {
    const uint8_t iv[16] = {0};
    return zf_crypto_aes256_cbc_encrypt(key, iv, input, output, size);
}

bool zf_crypto_aes256_cbc_zero_iv_decrypt(const uint8_t key[32], const uint8_t *input,
                                          uint8_t *output, size_t size) {
    const uint8_t iv[16] = {0};
    return zf_crypto_aes256_cbc_decrypt(key, iv, input, output, size);
}

bool zf_crypto_generate_key_agreement_key(ZfP256KeyAgreementKey *key) {
    memset(key, 0, sizeof(*key));
    memset(key->private_key, g_key_agreement_seed, sizeof(key->private_key));
    memset(key->public_x, g_key_agreement_seed, sizeof(key->public_x));
    memset(key->public_y, (uint8_t)(g_key_agreement_seed + 1), sizeof(key->public_y));
    g_key_agreement_seed++;
    return true;
}

bool zf_crypto_ecdh_shared_secret(const ZfP256KeyAgreementKey *key,
                                  const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                                  const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]) {
    UNUSED(key);
    UNUSED(peer_x);
    UNUSED(peer_y);
    memset(out, 0xAB, 32);
    return true;
}

bool zf_crypto_ecdh_raw_secret(const ZfP256KeyAgreementKey *key,
                               const uint8_t peer_x[ZF_PUBLIC_KEY_LEN],
                               const uint8_t peer_y[ZF_PUBLIC_KEY_LEN], uint8_t out[32]) {
    return zf_crypto_ecdh_shared_secret(key, peer_x, peer_y, out);
}

bool zf_crypto_generate_credential_keypair(ZfCredentialRecord *record) {
    memset(record->public_x, 0x11, sizeof(record->public_x));
    memset(record->public_y, 0x22, sizeof(record->public_y));
    memset(record->private_wrapped, 0x33, sizeof(record->private_wrapped));
    memset(record->private_iv, 0x44, sizeof(record->private_iv));
    return true;
}

bool zf_crypto_compute_public_key_from_private(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                               uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                               uint8_t public_y[ZF_PUBLIC_KEY_LEN]) {
    UNUSED(private_key);
    memset(public_x, 0x11, ZF_PUBLIC_KEY_LEN);
    memset(public_y, 0x22, ZF_PUBLIC_KEY_LEN);
    return true;
}

bool zf_crypto_sign_hash_with_private_key(const uint8_t private_key[ZF_PRIVATE_KEY_LEN],
                                          const uint8_t hash[32], uint8_t *out, size_t out_capacity,
                                          size_t *out_len) {
    UNUSED(private_key);
    UNUSED(hash);
    UNUSED(out_capacity);
    out[0] = 0x01;
    out[1] = 0x02;
    *out_len = 2;
    return true;
}

bool zf_crypto_verify_hash_with_public_key(const uint8_t public_x[ZF_PUBLIC_KEY_LEN],
                                           const uint8_t public_y[ZF_PUBLIC_KEY_LEN],
                                           const uint8_t hash[32], const uint8_t *signature,
                                           size_t signature_len) {
    UNUSED(public_x);
    UNUSED(public_y);
    UNUSED(hash);
    UNUSED(signature);
    UNUSED(signature_len);
    return g_crypto_verify_hash_result;
}

bool zf_crypto_sign_hash(const ZfCredentialRecord *record, const uint8_t hash[32], uint8_t *out,
                         size_t out_capacity, size_t *out_len) {
    UNUSED(record);
    UNUSED(hash);
    UNUSED(out_capacity);
    out[0] = 0xDE;
    out[1] = 0xAD;
    out[2] = 0xBE;
    out[3] = 0xEF;
    *out_len = 4;
    return true;
}

bool zf_crypto_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    uint8_t diff = 0;
    for (size_t i = 0; i < size; ++i) {
        diff |= left[i] ^ right[i];
    }
    return diff == 0;
}

FlipperFormat *flipper_format_file_alloc(Storage *storage) {
    static FlipperFormat flipper_format;

    UNUSED(storage);
    return &flipper_format;
}

void flipper_format_free(FlipperFormat *flipper_format) {
    UNUSED(flipper_format);
}

bool flipper_format_file_open_always(FlipperFormat *flipper_format, const char *path) {
    UNUSED(flipper_format);
    UNUSED(path);
    return false;
}

bool flipper_format_file_open_existing(FlipperFormat *flipper_format, const char *path) {
    UNUSED(flipper_format);
    UNUSED(path);
    return false;
}

bool flipper_format_file_close(FlipperFormat *flipper_format) {
    UNUSED(flipper_format);
    return true;
}

bool flipper_format_write_header_cstr(FlipperFormat *flipper_format, const char *type,
                                      uint32_t version) {
    UNUSED(flipper_format);
    UNUSED(type);
    UNUSED(version);
    return false;
}

bool flipper_format_write_uint32(FlipperFormat *flipper_format, const char *key,
                                 const uint32_t *value, size_t count) {
    UNUSED(flipper_format);
    UNUSED(key);
    UNUSED(value);
    UNUSED(count);
    return false;
}

bool flipper_format_write_hex(FlipperFormat *flipper_format, const char *key, const uint8_t *value,
                              size_t size) {
    UNUSED(flipper_format);
    UNUSED(key);
    UNUSED(value);
    UNUSED(size);
    return false;
}

bool flipper_format_read_header(FlipperFormat *flipper_format, FuriString *type,
                                uint32_t *version) {
    UNUSED(flipper_format);
    UNUSED(type);
    UNUSED(version);
    return false;
}

bool flipper_format_read_uint32(FlipperFormat *flipper_format, const char *key, uint32_t *value,
                                size_t count) {
    UNUSED(flipper_format);
    UNUSED(key);
    UNUSED(value);
    UNUSED(count);
    return false;
}

bool flipper_format_read_hex(FlipperFormat *flipper_format, const char *key, uint8_t *value,
                             size_t size) {
    UNUSED(flipper_format);
    UNUSED(key);
    UNUSED(value);
    UNUSED(size);
    return false;
}

FuriString *furi_string_alloc(void) {
    static FuriString string;

    memset(&string, 0, sizeof(string));
    return &string;
}

void furi_string_free(FuriString *string) {
    UNUSED(string);
}

const char *furi_string_get_cstr(const FuriString *string) {
    return string ? string->value : "";
}

void mbedtls_ecp_group_init(mbedtls_ecp_group *grp) {
    memset(grp, 0, sizeof(*grp));
}

void mbedtls_ecp_group_free(mbedtls_ecp_group *grp) {
    UNUSED(grp);
}

int mbedtls_ecp_group_load(mbedtls_ecp_group *grp, int id) {
    UNUSED(grp);
    UNUSED(id);
    return 0;
}

void mbedtls_ecp_point_init(mbedtls_ecp_point *pt) {
    memset(pt, 0, sizeof(*pt));
}

void mbedtls_ecp_point_free(mbedtls_ecp_point *pt) {
    UNUSED(pt);
}

int mbedtls_ecp_mul(mbedtls_ecp_group *grp, mbedtls_ecp_point *r, const mbedtls_mpi *m,
                    const mbedtls_ecp_point *p, int (*f_rng)(), void *p_rng) {
    UNUSED(grp);
    UNUSED(r);
    UNUSED(m);
    UNUSED(p);
    UNUSED(f_rng);
    UNUSED(p_rng);
    return 0;
}

int mbedtls_ecp_check_privkey(const mbedtls_ecp_group *grp, const mbedtls_mpi *d) {
    UNUSED(grp);
    UNUSED(d);
    return 0;
}

int mbedtls_ecp_check_pubkey(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt) {
    UNUSED(grp);
    UNUSED(pt);
    return 0;
}

int mbedtls_ecp_point_write_binary(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt,
                                   int format, size_t *olen, unsigned char *buf, size_t buflen) {
    UNUSED(grp);
    UNUSED(pt);
    UNUSED(format);
    if (buflen > 0) {
        memset(buf, 0, buflen);
        buf[0] = 0x04;
    }
    *olen = buflen;
    return 0;
}

int mbedtls_ecp_gen_keypair(mbedtls_ecp_group *grp, mbedtls_mpi *d, mbedtls_ecp_point *q,
                            int (*f_rng)(), void *p_rng) {
    UNUSED(grp);
    UNUSED(d);
    UNUSED(q);
    UNUSED(f_rng);
    UNUSED(p_rng);
    return 0;
}

int mbedtls_mpi_read_binary(mbedtls_mpi *x, const unsigned char *buf, size_t buflen) {
    UNUSED(x);
    UNUSED(buf);
    UNUSED(buflen);
    return 0;
}

int mbedtls_mpi_write_binary(const mbedtls_mpi *x, unsigned char *buf, size_t buflen) {
    UNUSED(x);
    memset(buf, 0, buflen);
    return 0;
}

int mbedtls_mpi_lset(mbedtls_mpi *x, int z) {
    UNUSED(x);
    UNUSED(z);
    return 0;
}

void mbedtls_mpi_init(mbedtls_mpi *x) {
    memset(x, 0, sizeof(*x));
}

void mbedtls_mpi_free(mbedtls_mpi *x) {
    UNUSED(x);
}

typedef int FuriStatus;

struct FuriMutex {
    int unused;
};

struct FuriSemaphore {
    int unused;
};

typedef enum {
    FuriMutexTypeNormal = 0,
} FuriMutexType;

#define FuriWaitForever 0U
#define FuriStatusOk 0

FuriMutex *furi_mutex_alloc(FuriMutexType type) {
    FuriMutex *mutex = malloc(sizeof(*mutex));

    UNUSED(type);
    if (mutex) {
        memset(mutex, 0, sizeof(*mutex));
    }
    return mutex;
}

void furi_mutex_free(FuriMutex *mutex) {
    free(mutex);
}

FuriStatus furi_mutex_acquire(FuriMutex *mutex, uint32_t timeout) {
    UNUSED(mutex);
    UNUSED(timeout);
    g_mutex_depth++;
    return FuriStatusOk;
}

void furi_mutex_release(FuriMutex *mutex) {
    UNUSED(mutex);
    if (g_mutex_depth > 0) {
        g_mutex_depth--;
    }
}

FuriSemaphore *furi_semaphore_alloc(uint32_t max_count, uint32_t initial_count) {
    FuriSemaphore *semaphore = malloc(sizeof(*semaphore));

    UNUSED(max_count);
    UNUSED(initial_count);
    if (semaphore) {
        memset(semaphore, 0, sizeof(*semaphore));
    }
    return semaphore;
}

void furi_semaphore_free(FuriSemaphore *semaphore) {
    free(semaphore);
}

uint32_t furi_get_tick(void) {
    return g_fake_tick;
}

FuriThreadId furi_thread_get_current_id(void) {
    return (FuriThreadId)0x1;
}

uint32_t furi_thread_get_stack_space(FuriThreadId thread_id) {
    UNUSED(thread_id);
    return 4096U;
}

FuriThread *furi_thread_alloc_ex(const char *name, size_t stack_size, FuriThreadCallback callback,
                                 void *context) {
    FuriThread *thread = NULL;

    UNUSED(name);
    if (g_thread_alloc_fail) {
        return NULL;
    }
    thread = malloc(sizeof(*thread));
    if (!thread) {
        return NULL;
    }
    memset(thread, 0, sizeof(*thread));
    thread->callback = callback;
    thread->context = context;
    thread->stack_size = stack_size;
    return thread;
}

void furi_thread_set_appid(FuriThread *thread, const char *appid) {
    UNUSED(thread);
    UNUSED(appid);
}

void furi_thread_set_priority(FuriThread *thread, FuriThreadPriority priority) {
    UNUSED(thread);
    UNUSED(priority);
}

void furi_thread_start(FuriThread *thread) {
    if (thread) {
        thread->started = true;
        g_thread_start_count++;
    }
}

void furi_thread_join(FuriThread *thread) {
    if (thread) {
        thread->joined = true;
        g_thread_join_count++;
    }
}

void furi_thread_free(FuriThread *thread) {
    if (thread) {
        g_thread_free_count++;
        free(thread);
    }
}

bool zerofido_ui_request_approval(ZerofidoApp *app, ZfUiProtocol protocol, const char *operation,
                                  const char *target_id, const char *user_text,
                                  uint32_t current_cid, bool *approved) {
    UNUSED(protocol);
    UNUSED(operation);
    UNUSED(target_id);
    UNUSED(user_text);
    UNUSED(current_cid);
    if (app &&
        (app->runtime_config.auto_accept_requests || app->transport_auto_accept_transaction)) {
        *approved = true;
        return true;
    }
    g_approval_request_count++;
    *approved = g_approval_result;
    return g_approval_request_ok;
}

bool zerofido_ui_request_assertion_selection(ZerofidoApp *app, const char *rp_id,
                                             const uint16_t *match_indices, size_t match_count,
                                             uint32_t current_cid,
                                             uint32_t *selected_record_index) {
    UNUSED(app);
    UNUSED(rp_id);
    UNUSED(current_cid);
    g_selection_request_count++;
    g_last_selection_match_count = match_count;
    if (!g_selection_request_ok || g_selection_index >= match_count) {
        return false;
    }

    *selected_record_index = match_indices[g_selection_index];
    return true;
}

ZfApprovalState zerofido_ui_get_interaction_state(ZerofidoApp *app) {
    UNUSED(app);
    return g_approval_state;
}

uint8_t zf_transport_poll_cbor_control(ZerofidoApp *app, uint32_t current_cid) {
    UNUSED(app);
    UNUSED(current_cid);
    if (g_transport_poll_requires_unlocked && g_mutex_depth != 0) {
        return ZF_CTAP_ERR_OTHER;
    }
    return g_transport_poll_status;
}

void zerofido_notify_success(ZerofidoApp *app) {
    UNUSED(app);
}

void zerofido_notify_error(ZerofidoApp *app) {
    UNUSED(app);
}

void zerofido_notify_reset(ZerofidoApp *app) {
    UNUSED(app);
}

void zerofido_ui_set_status(ZerofidoApp *app, const char *text) {
    UNUSED(app);
    g_status_update_count++;
    if (!text) {
        g_last_status_text[0] = '\0';
        return;
    }
    strncpy(g_last_status_text, text, sizeof(g_last_status_text) - 1);
    g_last_status_text[sizeof(g_last_status_text) - 1] = '\0';
}

bool zerofido_ui_init(ZerofidoApp *app) {
    UNUSED(app);
    return true;
}

void zerofido_ui_deinit(ZerofidoApp *app) {
    UNUSED(app);
}

void zerofido_ui_refresh_status_line(ZerofidoApp *app) {
    UNUSED(app);
}

void zerofido_ui_refresh_credentials_status(ZerofidoApp *app) {
    UNUSED(app);
}

static int32_t test_transport_worker(void *context) {
    UNUSED(context);
    return 0;
}

static void test_transport_stop(ZerofidoApp *app) {
    UNUSED(app);
}

const ZfTransportAdapterOps zf_transport_usb_hid_adapter = {
    .worker = test_transport_worker,
    .worker_stack_size = 6 * 1024,
    .stop = test_transport_stop,
};

const ZfTransportAdapterOps zf_transport_nfc_adapter = {
    .worker = test_transport_worker,
    .worker_stack_size = 4 * 1024,
    .stop = test_transport_stop,
};

void zf_transport_stop(ZerofidoApp *app) {
    test_transport_stop(app);
}

bool zerofido_notify_init(ZerofidoApp *app) {
    UNUSED(app);
    return true;
}

void zerofido_notify_deinit(ZerofidoApp *app) {
    UNUSED(app);
}

bool u2f_data_wipe(Storage *storage) {
    UNUSED(storage);
    return true;
}

bool zf_u2f_adapter_init(ZerofidoApp *app) {
    UNUSED(app);
    g_u2f_adapter_init_count++;
    return g_u2f_adapter_init_ok;
}

void zf_u2f_adapter_deinit(ZerofidoApp *app) {
    UNUSED(app);
    g_u2f_adapter_deinit_count++;
}

bool zf_u2f_adapter_is_available(const ZerofidoApp *app) {
    return app && g_u2f_adapter_init_ok;
}

#include "../../../src/zerofido_cbor.c"
#include "../../../src/ctap/parse/shared.c"
#include "../../../src/ctap/parse/get_assertion.c"
#include "../../../src/ctap/parse/make_credential.c"
#include "../../../src/pin/protocol.c"
#include "../../../src/ctap/extensions/cred_protect.c"
#include "../../../src/ctap/extensions/hmac_secret.c"
#include "../../../src/ctap/core/approval.c"
#include "../../../src/ctap/core/assertion_queue.c"
#include "../../../src/ctap/core/internal.c"
#include "../../../src/ctap/commands/get_assertion.c"
#include "../../../src/ctap/commands/make_credential.c"
#include "../../../src/ctap/commands/reset.c"
#include "../../../src/ctap/dispatch.c"
#include "../../../src/ctap/policy.c"
#include "../../../src/ctap/response.c"
#include "../../../src/pin/store/state_store.c"
#include "../../../src/pin/flow.c"
#include "../../../src/pin/core/token.c"
#include "../../../src/pin/core/retry.c"
#include "../../../src/pin/core/plaintext.c"
#include "../../../src/pin/core/lifecycle.c"
#include "../../../src/pin/client_pin/parse.c"
#include "../../../src/pin/client_pin/response.c"
#include "../../../src/pin/client_pin/operations.c"
#include "../../../src/pin/command.c"
#include "../../../src/zerofido_storage.c"
#include "../../../src/zerofido_runtime_config.c"
#include "../../../src/app/lifecycle.c"
#include "../../../src/store/bootstrap.c"
#include "../../../src/store/record_format.c"
#include "../../../src/store/recovery.c"
#include "../../../src/zerofido_store.c"
#include "../../../src/zerofido_ui_format.c"
#include "../../../src/zerofido_ctap_dispatch.c"

static void expect(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void test_init_descriptor_list(ZfCredentialDescriptorList *list,
                                      ZfCredentialDescriptor *entries, size_t capacity) {
    list->entries = entries;
    list->count = 0;
    list->capacity = capacity;
}

#define TEST_INIT_GET_ASSERTION_REQUEST(request)                                                   \
    ZfCredentialDescriptor request##_allow_descriptors[ZF_MAX_ALLOW_LIST];                         \
    test_init_descriptor_list(&(request).allow_list, request##_allow_descriptors, ZF_MAX_ALLOW_LIST)

#define TEST_INIT_MAKE_CREDENTIAL_REQUEST(request)                                                 \
    ZfCredentialDescriptor request##_exclude_descriptors[ZF_MAX_ALLOW_LIST];                       \
    test_init_descriptor_list(&(request).exclude_list, request##_exclude_descriptors,              \
                              ZF_MAX_ALLOW_LIST)

static bool test_zf_store_add_record(Storage *storage, ZfCredentialStore *store,
                                     const ZfCredentialRecord *record) {
    uint8_t buffer[ZF_STORE_RECORD_IO_SIZE];

    return zf_store_add_record_with_buffer(storage, store, record, buffer, sizeof(buffer));
}

static bool test_zf_store_init(Storage *storage, ZfCredentialStore *store) {
    uint8_t buffer[ZF_STORE_RECORD_IO_SIZE];

    return zf_store_init_with_buffer(storage, store, buffer, sizeof(buffer));
}

static bool test_zf_store_record_format_load_record(Storage *storage, const char *file_name,
                                                    ZfCredentialRecord *record) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];

    return zf_store_record_format_load_record_with_buffer(storage, file_name, record, buffer,
                                                          sizeof(buffer));
}

static bool test_zf_store_record_format_write_record(Storage *storage,
                                                     const ZfCredentialRecord *record) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];

    return zf_store_record_format_write_record_with_buffer(storage, record, buffer, sizeof(buffer));
}

static bool test_zf_record_decode(const uint8_t *data, size_t data_size, const char *file_name,
                                  ZfCredentialRecord *record) {
    ZfCounterFloorFile embedded_counter_floor;
    bool has_embedded_counter_floor = false;

    return zf_record_decode(data, data_size, file_name, record, &embedded_counter_floor,
                            &has_embedded_counter_floor);
}

#include "subsystems/app/tests.inc"
#include "subsystems/store/record_format_tests.inc"
#include "subsystems/cbor/tests.inc"
#include "subsystems/ctap/parse_tests.inc"
#include "subsystems/pin/tests.inc"
#include "subsystems/runtime/tests.inc"
#include "subsystems/pin/client_pin_tests.inc"
#include "subsystems/ctap/command_tests.inc"
#include "subsystems/store/atomic_counter_tests.inc"

int main(void) {
    test_command_scratch_is_allocated_lifetime_and_single_owner();
    test_record_decode_rejects_partial_record();
    test_record_decode_accepts_complete_record();
    test_record_encode_accepts_large_resident_record_with_hmac_secret();
    test_record_hmac_secret_storage_uses_wrapped_blob();
    test_record_decode_rejects_legacy_plaintext_hmac_secret_storage();
    test_record_decode_rejects_duplicate_hmac_secret_storage_fields();
    test_record_decode_rejects_embedded_nul_text();
    test_record_decode_rejects_file_name_mismatch();
    test_record_decode_rejects_oversized_version();
    test_record_decode_rejects_unsupported_record_version();
    test_store_init_ignores_unsupported_record_file();
    test_cbor_text_read_rejects_declared_length_past_buffer();
    test_cbor_skip_rejects_declared_length_past_buffer();
    test_cbor_skip_accepts_single_precision_float();
    test_cbor_read_uint_rejects_noncanonical_encoding();
    test_cbor_read_text_rejects_noncanonical_length();
    test_get_assertion_parse_treats_empty_allow_list_as_omitted();
    test_get_assertion_parse_skips_unknown_float_member();
    test_get_assertion_parse_rejects_duplicate_top_level_key();
    test_get_assertion_parse_rejects_duplicate_option_key();
    test_get_assertion_parse_ignores_unknown_true_option();
    test_get_assertion_parse_ignores_unknown_false_option();
    test_get_assertion_parse_rejects_zero_length_descriptor_id();
    test_get_assertion_parse_accepts_oversized_descriptor_id();
    test_get_assertion_parse_rejects_duplicate_descriptor_type_after_invalid_value();
    test_get_assertion_parse_rejects_embedded_nul_rp_id();
    test_get_assertion_parse_rejects_invalid_utf8_rp_id();
    test_get_assertion_parse_accepts_rk_false_option();
    test_get_assertion_parse_rejects_rk_true_option_with_unsupported_option();
    test_make_credential_parse_skips_unknown_simple_value();
    test_make_credential_parse_ignores_unknown_true_option();
    test_make_credential_parse_rejects_duplicate_user_name();
    test_make_credential_parse_accepts_64_byte_display_name();
    test_make_credential_parse_rejects_non_text_rp_name();
    test_make_credential_parse_rejects_non_text_rp_icon();
    test_make_credential_parse_rejects_non_text_user_icon();
    test_get_assertion_parse_accepts_253_byte_rp_id();
    test_assertion_response_user_fields_follow_uv();
    test_client_pin_parse_rejects_trailing_bytes();
    test_client_pin_get_retries_accepts_missing_pin_protocol();
    test_client_pin_get_key_agreement_requires_pin_protocol();
    test_client_pin_parse_rejects_duplicate_top_level_keys();
    test_client_pin_parse_rejects_duplicate_key_agreement_keys();
    test_client_pin_key_agreement_requires_alg();
    test_make_credential_parse_rejects_malformed_pubkey_cred_params();
    test_make_credential_parse_rejects_non_public_key_pubkey_cred_param_as_unsupported_algorithm();
    test_make_credential_parse_rejects_zero_length_exclude_id();
    test_make_credential_parse_rejects_duplicate_exclude_descriptors();
    test_make_credential_parse_accepts_oversized_exclude_id();
    test_make_credential_parse_ignores_non_public_key_exclude_descriptor();
    test_make_credential_parse_rejects_empty_user_id();
    test_get_assertion_parse_rejects_duplicate_allow_list_descriptors();
    test_get_assertion_parse_rejects_too_many_allow_list_descriptors();
    test_get_assertion_parse_rejects_duplicate_oversized_allow_list_descriptors();
    test_get_assertion_parse_ignores_non_public_key_allow_list_descriptor();
    test_store_find_by_rp_orders_records_by_newest_first();
    test_store_descriptor_list_ignores_oversized_descriptor_ids();
    test_pin_auth_block_state_clears_on_reinit_but_keeps_retries();
    test_pin_plaintext_accepts_ctap_max_utf8_length();
    test_pin_resume_auth_attempts_clears_persisted_block();
    test_pin_clear_removes_persisted_state_and_resets_runtime();
    test_pin_init_rejects_unsupported_format_state();
    test_pin_init_rejects_unsupported_current_format_version();
    test_pin_init_rejects_tampered_retry_state();
    test_pin_auth_blocks_after_three_mismatches();
    test_ui_format_approval_header_prefixes_protocol();
    test_ui_format_approval_body_uses_protocol_specific_target_label();
    test_ui_format_fido2_credential_label_uses_passkey_text();
    test_ui_format_fido2_credential_detail_uses_user_facing_text();
    test_ui_hex_encode_truncated_limits_output();
    test_get_pin_token_wrong_pin_regenerates_key_agreement();
    test_get_key_agreement_does_not_rotate_runtime_secrets();
    test_get_pin_token_success_rotates_pin_token();
    test_get_pin_token_experimental_2_1_requires_consent();
    test_get_pin_token_protocol2_returns_iv_prefixed_token();
    test_client_pin_protocol2_requires_experimental_profile();
    test_get_pin_token_decrypt_failure_consumes_retry();
    test_get_pin_token_rejects_permissions_fields();
    test_client_pin_permissions_subcommand_requires_permissions();
    test_client_pin_permissions_subcommand_rejects_zero_permissions();
    test_client_pin_permissions_subcommand_rejects_unsupported_permissions();
    test_client_pin_permissions_subcommand_requires_rp_id_for_mc_ga();
    test_client_pin_permissions_subcommand_experimental_validates_parameters();
    test_client_pin_permissions_subcommand_stores_permissions_and_rp_id();
    test_client_pin_permissions_subcommand_issued_token_consumes_mc_after_up();
    test_client_pin_permissions_subcommand_denied_consent_does_not_issue_token();
    test_client_pin_permissions_subcommand_rejects_unsupported_be_permission();
    test_pin_auth_rejects_expired_pin_token();
    test_pin_auth_protocol2_accepts_full_hmac();
    test_legacy_pin_token_allows_reuse_without_rp_binding();
    test_pin_auth_rejects_missing_required_permission();
    test_pin_auth_rejects_scoped_token_for_wrong_rp();
    test_change_pin_invalid_new_pin_after_correct_current_pin_resets_retries();
    test_change_pin_preserves_key_agreement_on_success();
    test_set_pin_invalid_new_pin_block_is_rejected();
    test_set_pin_new_pin_decrypt_failure_returns_pin_auth_invalid();
    test_set_pin_rejects_oversized_new_pin_block();
    test_set_pin_preserves_key_agreement_on_success();
    test_set_pin_creates_app_data_dir_before_persisting();
    test_change_pin_pin_hash_decrypt_failure_consumes_retry();
    test_change_pin_new_pin_decrypt_failure_returns_pin_auth_invalid();
    test_pin_init_cleans_temp_file();
    test_store_resident_records_for_same_user_coexist_after_reload();
    test_store_wipe_app_data_clears_credentials_and_pin_state();
    test_runtime_config_load_defaults_auto_accept_off();
    test_runtime_config_persist_round_trips_auto_accept_setting();
    test_runtime_config_persist_round_trips_transport_mode();
    test_runtime_config_persist_round_trips_fido2_profile();
    test_runtime_config_persist_round_trips_attestation_mode();
    test_runtime_config_set_attestation_mode_updates_capabilities();
    test_runtime_config_set_fido2_profile_updates_capabilities();
    test_runtime_config_set_fido2_profile_requires_pin_for_2_1();
    test_runtime_config_apply_downgrades_2_1_when_pin_is_unset();
    test_runtime_config_pin_refresh_restores_requested_2_1_profile();
    test_runtime_config_persist_preserves_requested_2_1_after_pin_clear();
    test_runtime_config_load_invalid_file_falls_back_to_defaults();
    test_runtime_config_load_rejects_unsupported_version();
    test_runtime_config_set_auto_accept_preserves_runtime_state_on_persist_failure();
    test_runtime_config_set_fido2_preserves_runtime_state_on_persist_failure();
    test_runtime_config_set_fido2_profile_preserves_runtime_state_on_persist_failure();
    test_runtime_config_set_attestation_preserves_runtime_state_on_persist_failure();
    test_transport_mode_switch_failure_preserves_persisted_mode();
    test_ctap_error_response_omits_body_when_store_add_fails();
    test_make_credential_rejects_local_maintenance_window();
    test_make_credential_empty_pin_auth_requires_pin_protocol();
    test_get_assertion_empty_pin_auth_requires_pin_protocol();
    test_make_credential_empty_pin_auth_probe_returns_pin_status_after_touch();
    test_get_assertion_empty_pin_auth_probe_returns_pin_status_after_touch();
    test_get_assertion_uv_without_pin_auth_returns_unsupported_option();
    test_make_credential_uv_without_pin_auth_returns_unsupported_option();
    test_make_credential_pin_auth_takes_precedence_over_uv();
    test_make_credential_pin_auth_clears_permission_scoped_token_after_up();
    test_make_credential_legacy_pin_token_keeps_mc_ga_after_up();
    test_make_credential_allows_pinless_non_resident_when_pin_is_set();
    test_make_credential_requires_pin_auth_for_resident_when_pin_is_set();
    test_make_credential_2_1_make_cred_uv_not_required_allows_pinless_non_resident();
    test_make_credential_2_1_make_cred_uv_not_required_rejects_resident_without_pin_auth();
    test_make_credential_auto_accept_bypasses_approval_prompt();
    test_make_credential_unknown_option_succeeds();
    #if ZF_PACKED_ATTESTATION
    test_make_credential_returns_packed_attestation();
    #else
    test_make_credential_packed_off_returns_none_attestation();
    #endif
    test_make_credential_runtime_none_returns_none_attestation();
    test_make_credential_attestation_preference_overrides_runtime_default();
    #if ZF_PACKED_ATTESTATION
    test_make_credential_attestation_preference_uses_lowest_supported_index();
    test_make_credential_response_does_not_downgrade_required_packed();
    #endif
    test_make_credential_nfc_auto_accept_keeps_uv_clear();
    test_make_credential_nfc_auto_accept_rejects_uv_option_without_pin();
    test_get_assertion_pin_auth_takes_precedence_over_uv();
    test_get_assertion_pin_auth_clears_permission_scoped_token_after_up();
    test_permission_scoped_pin_token_rejects_browser_style_replay_after_mc();
    test_get_assertion_legacy_pin_token_keeps_mc_ga_after_up();
    test_get_assertion_allows_discouraged_uv_when_pin_is_set();
    test_get_assertion_auto_accept_bypasses_approval_prompt();
    test_get_assertion_discovers_no_credentials_without_uv_for_cred_protect_2();
    test_get_assertion_with_pin_auth_allows_discoverable_cred_protect_2();
    test_make_credential_overwrites_resident_credential_for_same_user();
    test_make_credential_cred_protect_round_trips_into_auth_data_and_store();
    test_make_credential_hmac_secret_round_trips_into_auth_data_and_store();
    test_get_assertion_hmac_secret_builds_protocol1_output();
    test_assertion_response_preserves_scratch_hmac_secret_extension();
    test_make_credential_excluded_credential_returns_excluded_after_timeout();
    test_make_credential_exclude_list_hides_uv_required_credential_without_uv();
    test_make_credential_exclude_list_reveals_uv_required_credential_with_pin_auth();
    test_get_assertion_without_matching_credential_waits_for_approval_before_no_credentials();
    test_get_assertion_invalid_allow_list_waits_for_approval_before_no_credentials();
    test_get_assertion_multi_match_uses_account_selection();
    test_get_assertion_multi_match_experimental_2_1_includes_user_selected();
    test_get_assertion_up_false_multi_match_reports_count_and_queues_next();
    test_get_assertion_multi_match_selection_denied_returns_operation_denied();
    test_get_assertion_multi_match_selection_cancel_returns_keepalive_cancel();
    test_get_assertion_multi_match_selection_timeout_returns_operation_denied();
    test_get_assertion_multi_match_keeps_account_selection_when_auto_accept_is_on();
    test_get_assertion_allow_list_does_not_open_account_selection();
    test_get_next_assertion_rejects_wrong_cid();
    test_get_next_assertion_refreshes_queue_expiry();
    test_get_next_assertion_rejects_empty_queue();
    test_get_next_assertion_rejects_expired_queue();
    test_get_next_assertion_clears_final_queue_state();
    test_get_next_assertion_missing_record_clears_queue();
    test_get_next_assertion_rejects_trailing_payload();
    test_get_info_rejects_trailing_payload();
    test_get_info_success_updates_status_banner();
    test_get_info_advertises_cred_protect_extension();
    test_get_info_advertises_hmac_secret_extension();
    test_get_info_advertises_fido_2_0_without_fido_2_1();
    test_get_info_nfc_ctap2_0_advertises_transport_compactly();
    test_get_info_experimental_2_1_advertises_pin_uv_auth_token();
    test_get_info_advertises_u2f();
    test_get_info_reports_firmware_version();
    test_ctap_reset_succeeds_and_wipes_runtime_state();
    test_ctap_reset_auto_accept_bypasses_approval_prompt();
    test_ctap_reset_succeeds_when_u2f_reinit_fails();
    test_ctap_reset_rejects_trailing_payload();
    test_client_pin_get_key_agreement_updates_status_banner();
    test_selection_touch_succeeds_and_updates_status_banner();
    test_selection_touch_rejects_default_ctap2_0_profile();
    test_selection_touch_auto_accept_bypasses_approval_prompt();
    test_store_cleanup_restores_backup_when_primary_is_missing();
    test_store_cleanup_restores_backup_when_primary_is_corrupt();
    test_store_cleanup_keeps_current_counter_when_backup_exists();
    test_store_remove_record_paths_deletes_atomic_backups();
    test_atomic_file_write_overwrites_primary_without_predelete();
    test_atomic_file_write_failure_keeps_primary();
    test_atomic_file_recovery_restores_backup_when_primary_missing();
    test_atomic_file_remove_deletes_primary_temp_and_backup();
    test_pin_auth_mismatch_keeps_block_state_when_persist_fails();
    test_wrong_pin_keeps_retry_state_when_persist_fails();
    test_pin_persist_failure_poison_blocks_reinit_after_wrong_pin();
    test_pin_persist_failure_poison_blocks_reinit_after_pin_auth_mismatch();
    test_pin_persist_failure_falls_back_to_remove_when_poison_write_fails();
    test_correct_pin_auth_keeps_retry_state_when_persist_fails();
    test_get_assertion_polls_control_without_holding_ui_mutex();
    test_get_next_assertion_polls_control_without_holding_ui_mutex();
    test_store_add_record_failure_keeps_original_record();
    test_store_file_advances_record_rollback_to_counter_high_water();
    test_store_file_preserves_reserved_floor_when_counter_file_is_missing();
    test_store_file_bootstraps_legacy_record_without_embedded_counter_floor();
    test_store_file_rejects_corrupt_embedded_counter_floor();
    test_store_file_external_counter_floor_wins_over_embedded_floor();
    test_store_advance_counter_uses_reserved_counter_window();
    test_store_update_record_reserves_counter_floor_before_counter_advance();
    test_store_file_write_and_load_fail_closed_without_counter_floor_crypto();
    test_store_counter_reservation_failure_does_not_publish_counter();
    puts("native protocol regressions passed");
    return 0;
}
