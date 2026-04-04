#include "miniz_tinfl.h"
#include "memzero.h"
#include "kdbx_protected.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef FURI_LOG_T
#undef FURI_LOG_T
#endif
#define FURI_LOG_T(...) \
    do {                \
    } while(0)

#define TINFL_DEBUG_LOG(...) \
    do {                     \
    } while(0)

#ifndef MINIZ_NO_INFLATE_APIS

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define TINFL_RUNTIME_ALLOCA(size) __builtin_alloca(size)
#else
#define TINFL_RUNTIME_ALLOCA(size) NULL
#endif

#define TINFL_CR_BEGIN            \
    switch(r->m_state) {          \
    case 0:
#define TINFL_CR_RETURN(state_index, result) \
    do {                                       \
        status = result;                       \
        r->m_state = state_index;              \
        goto common_exit;                      \
    case state_index:;                         \
    } while(0)
#define TINFL_CR_RETURN_FOREVER(state_index, result) \
    do {                                             \
        for(;;) {                                    \
            TINFL_CR_RETURN(state_index, result);    \
        }                                            \
    } while(0)
#define TINFL_CR_FINISH }

#define TINFL_GET_BYTE(state_index, c)                                                              \
    do {                                                                                            \
        if(!tinfl_paged_runtime_heartbeat(runtime)) {                                               \
            status = TINFL_STATUS_FAILED;                                                           \
            goto common_exit;                                                                       \
        }                                                                                           \
        while(pIn_buf_cur >= pIn_buf_end) {                                                         \
            TINFL_CR_RETURN(                                                                        \
                state_index,                                                                        \
                (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT :       \
                                                              TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS); \
        }                                                                                           \
        c = *pIn_buf_cur++;                                                                         \
    } while(0)

#define TINFL_NEED_BITS(state_index, n)             \
    do {                                            \
        mz_uint c;                                  \
        TINFL_GET_BYTE(state_index, c);             \
        bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); \
        num_bits += 8;                              \
    } while(num_bits < (mz_uint)(n))
#define TINFL_SKIP_BITS(state_index, n)   \
    do {                                  \
        if(num_bits < (mz_uint)(n)) {     \
            TINFL_NEED_BITS(state_index, n); \
        }                                 \
        bit_buf >>= (n);                  \
        num_bits -= (n);                  \
    } while(0)
#define TINFL_GET_BITS(state_index, b, n) \
    do {                                  \
        if(num_bits < (mz_uint)(n)) {     \
            TINFL_NEED_BITS(state_index, n); \
        }                                 \
        b = bit_buf & ((1 << (n)) - 1);   \
        bit_buf >>= (n);                  \
        num_bits -= (n);                  \
    } while(0)

#define TINFL_HUFF_BITBUF_FILL(state_index, pLookUp, pTree)         \
    do {                                                            \
        temp = pLookUp[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)];     \
        if(temp >= 0) {                                             \
            code_len = temp >> 9;                                   \
            if((code_len) && (num_bits >= code_len)) break;         \
        } else if(num_bits > TINFL_FAST_LOOKUP_BITS) {              \
            code_len = TINFL_FAST_LOOKUP_BITS;                      \
            do {                                                    \
                temp = pTree[~temp + ((bit_buf >> code_len++) & 1)]; \
            } while((temp < 0) && (num_bits >= (code_len + 1)));    \
            if(temp >= 0) break;                                    \
        }                                                           \
        TINFL_GET_BYTE(state_index, c);                             \
        bit_buf |= (((tinfl_bit_buf_t)c) << num_bits);              \
        num_bits += 8;                                              \
    } while(num_bits < 15)

#define TINFL_HUFF_DECODE(state_index, sym, pLookUp, pTree)                    \
    do {                                                                       \
        int temp;                                                              \
        mz_uint code_len, c;                                                   \
        if(num_bits < 15) {                                                    \
            if((pIn_buf_end - pIn_buf_cur) < 2) {                              \
                TINFL_HUFF_BITBUF_FILL(state_index, pLookUp, pTree);           \
            } else {                                                           \
                bit_buf |= (((tinfl_bit_buf_t)pIn_buf_cur[0]) << num_bits) |   \
                           (((tinfl_bit_buf_t)pIn_buf_cur[1]) << (num_bits + 8)); \
                pIn_buf_cur += 2;                                              \
                num_bits += 16;                                                \
            }                                                                  \
        }                                                                      \
        if((temp = pLookUp[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) {    \
            code_len = temp >> 9;                                              \
            temp &= 511;                                                       \
        } else {                                                               \
            code_len = TINFL_FAST_LOOKUP_BITS;                                 \
            do {                                                               \
                temp = pTree[~temp + ((bit_buf >> code_len++) & 1)];           \
            } while(temp < 0);                                                 \
        }                                                                      \
        sym = temp;                                                            \
        bit_buf >>= code_len;                                                  \
        num_bits -= code_len;                                                  \
    } while(0)

typedef struct {
    mz_uint8* pages[TINFL_PAGED_LZ_DICT_PAGE_COUNT];
} tinfl_paged_dict;

typedef struct tinfl_paged_runtime_tag tinfl_paged_runtime;

typedef struct {
    mz_uint8 (*get)(const void* impl, size_t offset);
    void (*set)(void* impl, size_t offset, mz_uint8 value);
    void (*write)(void* impl, size_t offset, const mz_uint8* src, size_t len);
    int (*flush)(void* impl, size_t offset, size_t len, tinfl_put_buf_func_ptr callback, void* user);
    bool (*copy_match)(
        void* impl,
        size_t dst_offset,
        size_t src_offset,
        size_t len,
        tinfl_paged_runtime* runtime);
    bool (*failed)(const void* impl);
} tinfl_dict_ops;

typedef struct {
    const tinfl_dict_ops* ops;
    void* impl;
} tinfl_dict_view;

struct tinfl_paged_runtime_tag {
    tinfl_paged_telemetry* telemetry;
    size_t output_since_yield;
    size_t heartbeat_counter;
    size_t io_touch_counter;
    size_t next_trace_output;
    uint32_t deadline_tick;
};

#define TINFL_PAGED_YIELD_OUTPUT_BYTES 8192U
#define TINFL_PAGED_HEARTBEAT_STEPS    131072U
#define TINFL_PAGED_IO_TOUCH_STEPS     64U
#define TINFL_PAGED_TIMEOUT_MS         300000U
#define TINFL_PAGED_INPUT_CHUNK_BYTES  512U
#define TINFL_FILE_DICT_MAC_SIZE       32U
#define TINFL_FILE_DICT_MAX_CACHE_PAGES 1U
#define TINFL_FILE_DICT_MIN_CACHE_PAGES 1U
#define TINFL_FILE_DICT_USE_COPY_MATCH 1U
#define TINFL_FILE_TAG                 "FlipPassGzip"

typedef struct {
    uint8_t* data;
    uint16_t page_index;
    bool valid;
    bool dirty;
    uint32_t age;
} tinfl_file_cache_page;

typedef struct {
    Storage* storage;
    bool owns_storage;
    const char* file_path;
    File* file;
    tinfl_paged_telemetry* telemetry;
    tinfl_paged_runtime* runtime;
    tinfl_file_cache_page cache[TINFL_FILE_DICT_MAX_CACHE_PAGES];
    size_t cache_pages;
    uint32_t age_counter;
    bool storage_failed;
    bool fatal_failed;
    const char* storage_stage;
    bool page_initialized[TINFL_PAGED_LZ_DICT_PAGE_COUNT];
    uint8_t session_master[32];
    uint8_t enc_key[32];
    uint8_t mac_key[32];
    uint8_t nonce_prefix[4];
    uint8_t* io_page;
    uint16_t io_page_index;
    bool io_page_valid;
    uint8_t io_mac[TINFL_FILE_DICT_MAC_SIZE];
    size_t diag_acquire_count;
    size_t diag_set_count;
    size_t diag_get_count;
    size_t diag_write_count;
    size_t diag_flush_count;
    size_t diag_page_read_count;
    size_t diag_page_write_count;
} tinfl_file_dict;

static tinfl_file_cache_page* tinfl_file_dict_acquire_page(
    tinfl_file_dict* dict,
    uint16_t page_index,
    bool for_write,
    tinfl_paged_telemetry* telemetry);

static bool tinfl_paged_tick_expired(uint32_t now, uint32_t deadline) {
    return ((int32_t)(now - deadline)) >= 0;
}

static void tinfl_paged_telemetry_reset(tinfl_paged_telemetry* telemetry) {
    if(telemetry == NULL) {
        return;
    }

    const size_t trace_interval_bytes = telemetry->trace_interval_bytes;
    tinfl_paged_trace_func_ptr trace_callback = telemetry->trace_callback;
    void* trace_context = telemetry->trace_context;
    memset(telemetry, 0, sizeof(*telemetry));
    telemetry->page_size = TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    telemetry->page_count = TINFL_PAGED_LZ_DICT_PAGE_COUNT;
    telemetry->cache_pages = TINFL_PAGED_LZ_DICT_PAGE_COUNT;
    telemetry->timeout_ms = TINFL_PAGED_TIMEOUT_MS;
    telemetry->trace_interval_bytes = trace_interval_bytes;
    telemetry->failed_page_index = (size_t)-1;
    telemetry->last_status = TINFL_STATUS_FAILED;
    telemetry->trace_callback = trace_callback;
    telemetry->trace_context = trace_context;
    telemetry->storage_stage = NULL;
    telemetry->storage_failed = 0;
}

static void tinfl_debug_trace_telemetry(tinfl_paged_telemetry* telemetry, const char* event) {
    if(telemetry == NULL || telemetry->trace_callback == NULL || event == NULL) {
        return;
    }

    telemetry->trace_callback(event, telemetry, telemetry->trace_context);
}

#if FLIPPASS_ENABLE_GZIP_PAGED_TRACE
static void tinfl_paged_trace(tinfl_paged_runtime* runtime, const char* event) {
    if(runtime == NULL || runtime->telemetry == NULL || runtime->telemetry->trace_callback == NULL) {
        return;
    }

    runtime->telemetry->trace_callback(
        event, runtime->telemetry, runtime->telemetry->trace_context);
}

static void tinfl_paged_trace_telemetry(tinfl_paged_telemetry* telemetry, const char* event) {
    if(telemetry == NULL || telemetry->trace_callback == NULL) {
        return;
    }

    telemetry->trace_callback(event, telemetry, telemetry->trace_context);
}

static void tinfl_file_dict_trace_step(
    tinfl_file_dict* dict,
    tinfl_paged_telemetry* telemetry,
    const char* event) {
    tinfl_paged_telemetry* active = telemetry;
    if(active == NULL && dict != NULL) {
        active = dict->telemetry;
    }
    tinfl_paged_trace_telemetry(active, event);
}

static void tinfl_file_dict_trace_runtime(tinfl_file_dict* dict, const char* event) {
    if(dict == NULL || dict->runtime == NULL) {
        return;
    }

    tinfl_paged_trace(dict->runtime, event);
}
#else
#define tinfl_paged_trace(runtime, event) \
    do {                                  \
        UNUSED(runtime);                  \
    } while(0)
#define tinfl_paged_trace_telemetry(telemetry, event) \
    do {                                              \
        UNUSED(telemetry);                            \
    } while(0)
#define tinfl_file_dict_trace_step(dict, telemetry, event) \
    do {                                                  \
        UNUSED(dict);                                     \
        UNUSED(telemetry);                                \
    } while(0)
#define tinfl_file_dict_trace_runtime(dict, event) \
    do {                                           \
        UNUSED(dict);                              \
    } while(0)
#endif

static bool tinfl_paged_runtime_note_output(tinfl_paged_runtime* runtime, size_t output_bytes) {
    if(runtime == NULL || runtime->telemetry == NULL || output_bytes == 0U) {
        return true;
    }

    runtime->telemetry->output_bytes += output_bytes;
    runtime->output_since_yield += output_bytes;
    if(runtime->telemetry->trace_interval_bytes > 0U &&
       runtime->telemetry->output_bytes >= runtime->next_trace_output) {
        tinfl_paged_trace(runtime, "progress");
        do {
            runtime->next_trace_output += runtime->telemetry->trace_interval_bytes;
        } while(runtime->telemetry->output_bytes >= runtime->next_trace_output);
    }

    if(runtime->output_since_yield < TINFL_PAGED_YIELD_OUTPUT_BYTES) {
        return true;
    }

    runtime->output_since_yield = 0U;
    runtime->telemetry->yield_count++;
    furi_thread_yield();

    if(tinfl_paged_tick_expired(furi_get_tick(), runtime->deadline_tick)) {
        runtime->telemetry->timed_out = 1;
        return false;
    }

    return true;
}

static bool tinfl_paged_runtime_heartbeat(tinfl_paged_runtime* runtime) {
    if(runtime == NULL || runtime->telemetry == NULL) {
        return true;
    }

    runtime->heartbeat_counter++;
    if(runtime->heartbeat_counter < TINFL_PAGED_HEARTBEAT_STEPS) {
        return true;
    }

    runtime->heartbeat_counter = 0U;
    runtime->telemetry->yield_count++;
    furi_thread_yield();

    if(tinfl_paged_tick_expired(furi_get_tick(), runtime->deadline_tick)) {
        runtime->telemetry->timed_out = 1;
        return false;
    }

    return true;
}

static bool tinfl_paged_runtime_yield_io(tinfl_paged_runtime* runtime) {
    if(runtime == NULL || runtime->telemetry == NULL) {
        return true;
    }

    runtime->telemetry->yield_count++;
    furi_thread_yield();

    if(tinfl_paged_tick_expired(furi_get_tick(), runtime->deadline_tick)) {
        runtime->telemetry->timed_out = 1;
        return false;
    }

    return true;
}

static bool tinfl_file_cleanup_file(Storage* storage, const char* path) {
    if(storage == NULL || path == NULL || !storage_file_exists(storage, path)) {
        return true;
    }

    File* file = storage_file_alloc(storage);
    if(file == NULL) {
        return false;
    }

    bool ok = true;
    if(storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        uint64_t remaining = storage_file_size(file);
        uint8_t wipe[256];
        memset(wipe, 0, sizeof(wipe));
        storage_file_seek(file, 0U, true);

        while(remaining > 0U && ok) {
            const size_t chunk = remaining > sizeof(wipe) ? sizeof(wipe) : (size_t)remaining;
            ok = storage_file_write(file, wipe, chunk) == chunk;
            remaining -= chunk;
        }

        if(ok) {
            storage_file_sync(file);
            storage_file_seek(file, 0U, true);
            storage_file_truncate(file);
        }

        storage_file_close(file);
    } else {
        ok = false;
    }

    storage_file_free(file);
    storage_simply_remove(storage, path);
    return ok;
}

static void tinfl_file_dict_note_failure(
    tinfl_file_dict* dict,
    tinfl_paged_telemetry* telemetry,
    const char* stage) {
    if(dict != NULL) {
        dict->storage_failed = true;
        dict->fatal_failed = true;
        dict->storage_stage = stage;
    }
    if(telemetry != NULL) {
        telemetry->storage_failed = 1;
        telemetry->storage_stage = stage;
    }
}

static void tinfl_file_dict_note_budget_issue(
    tinfl_file_dict* dict,
    tinfl_paged_telemetry* telemetry,
    const char* stage) {
    if(dict != NULL) {
        dict->fatal_failed = true;
        dict->storage_stage = stage;
    }
    if(telemetry != NULL) {
        telemetry->storage_failed = 0;
        telemetry->storage_stage = stage;
    }
}

static void tinfl_file_dict_derive_keys(tinfl_file_dict* dict) {
    uint8_t hash[64];

    furi_assert(dict);

    sha512_Raw(dict->session_master, sizeof(dict->session_master), hash);
    memcpy(dict->enc_key, hash, sizeof(dict->enc_key));
    memcpy(dict->mac_key, hash + sizeof(dict->enc_key), sizeof(dict->mac_key));
    memzero(hash, sizeof(hash));
}

static void tinfl_file_dict_nonce(
    const tinfl_file_dict* dict,
    uint16_t page_index,
    uint8_t nonce[12]) {
    furi_assert(dict);
    furi_assert(nonce);

    memcpy(nonce, dict->nonce_prefix, sizeof(dict->nonce_prefix));
    for(size_t index = 0; index < 8U; index++) {
        nonce[4U + index] = (uint8_t)(((uint64_t)page_index >> (index * 8U)) & 0xFFU);
    }
}

static void tinfl_file_dict_mac(
    const tinfl_file_dict* dict,
    uint16_t page_index,
    const uint8_t* ciphertext,
    uint8_t mac[TINFL_FILE_DICT_MAC_SIZE]) {
    HMAC_SHA256_CTX hmac_ctx;
    uint8_t page_le[4];

    furi_assert(dict);
    furi_assert(ciphertext);
    furi_assert(mac);

    page_le[0] = (uint8_t)(page_index & 0xFFU);
    page_le[1] = (uint8_t)((page_index >> 8U) & 0xFFU);
    page_le[2] = 0U;
    page_le[3] = 0U;

    hmac_sha256_Init(&hmac_ctx, dict->mac_key, sizeof(dict->mac_key));
    hmac_sha256_Update(&hmac_ctx, page_le, sizeof(page_le));
    hmac_sha256_Update(&hmac_ctx, ciphertext, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    hmac_sha256_Final(&hmac_ctx, mac);
}

static uint32_t tinfl_file_dict_slot_offset(uint16_t page_index) {
    return (uint32_t)page_index * (TINFL_PAGED_LZ_DICT_PAGE_SIZE + TINFL_FILE_DICT_MAC_SIZE);
}

static bool tinfl_file_dict_runtime_touch(
    tinfl_file_dict* dict,
    tinfl_paged_telemetry* telemetry,
    const char* stage) {
    if(dict == NULL || dict->runtime == NULL || dict->runtime->telemetry == NULL) {
        return true;
    }

    dict->runtime->io_touch_counter++;
    if(dict->runtime->io_touch_counter < TINFL_PAGED_IO_TOUCH_STEPS) {
        if(tinfl_paged_tick_expired(furi_get_tick(), dict->runtime->deadline_tick)) {
            dict->runtime->telemetry->timed_out = 1;
            tinfl_file_dict_note_budget_issue(dict, telemetry, stage);
            return false;
        }
        return true;
    }

    dict->runtime->io_touch_counter = 0U;
    if(!tinfl_paged_runtime_yield_io(dict->runtime)) {
        tinfl_file_dict_note_budget_issue(dict, telemetry, stage);
        return false;
    }

    return true;
}

static bool tinfl_file_dict_seek_page(
    tinfl_file_dict* dict,
    uint16_t page_index,
    tinfl_paged_telemetry* telemetry,
    const char* stage) {
    if(dict == NULL || dict->file == NULL) {
        tinfl_file_dict_note_failure(dict, telemetry, stage);
        return false;
    }

    if(!tinfl_file_dict_runtime_touch(dict, telemetry, stage)) {
        return false;
    }

    if(!storage_file_seek(dict->file, tinfl_file_dict_slot_offset(page_index), true)) {
        tinfl_file_dict_note_failure(dict, telemetry, stage);
        return false;
    }

    return tinfl_file_dict_runtime_touch(dict, telemetry, stage);
}

static bool tinfl_file_dict_load_plain_page_to_buffer(
    tinfl_file_dict* dict,
    uint16_t page_index,
    uint8_t* out,
    tinfl_paged_telemetry* telemetry,
    const char* seek_stage,
    const char* io_stage) {
    uint8_t expected_mac[TINFL_FILE_DICT_MAC_SIZE];
    uint8_t nonce[12];

    furi_assert(dict);
    furi_assert(out);

    if(!dict->page_initialized[page_index]) {
        memset(out, 0, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        return true;
    }

    if(dict->file == NULL) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_open_read");
        return false;
    }

    if(!tinfl_file_dict_seek_page(dict, page_index, telemetry, seek_stage)) {
        return false;
    }

    if(storage_file_read(dict->file, out, TINFL_PAGED_LZ_DICT_PAGE_SIZE) != TINFL_PAGED_LZ_DICT_PAGE_SIZE ||
       !tinfl_file_dict_runtime_touch(dict, telemetry, io_stage) ||
       storage_file_read(dict->file, dict->io_mac, sizeof(dict->io_mac)) != sizeof(dict->io_mac) ||
       !tinfl_file_dict_runtime_touch(dict, telemetry, io_stage)) {
        tinfl_file_dict_note_failure(dict, telemetry, io_stage);
        return false;
    }

    tinfl_file_dict_mac(dict, page_index, out, expected_mac);
    if(memcmp(expected_mac, dict->io_mac, sizeof(expected_mac)) != 0) {
        memzero(expected_mac, sizeof(expected_mac));
        tinfl_file_dict_note_failure(dict, telemetry, "window_verify");
        return false;
    }
    memzero(expected_mac, sizeof(expected_mac));

    tinfl_file_dict_nonce(dict, page_index, nonce);
    if(!kdbx_chacha20_xor(
           out,
           TINFL_PAGED_LZ_DICT_PAGE_SIZE,
           dict->enc_key,
           sizeof(dict->enc_key),
           nonce,
           sizeof(nonce),
           0U)) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_decrypt");
        return false;
    }

    return true;
}

static bool tinfl_file_dict_write_page(
    tinfl_file_dict* dict,
    uint16_t page_index,
    uint8_t* plain,
    tinfl_paged_telemetry* telemetry) {
    uint8_t nonce[12];
    bool ok = false;
    bool encrypted = false;

    furi_assert(dict);
    furi_assert(plain);

    if(dict->diag_page_write_count++ < 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_begin");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "page_write_begin page=%u free=%lu max=%lu",
            page_index,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }

    tinfl_file_dict_nonce(dict, page_index, nonce);
    memcpy(dict->io_page, plain, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    if(dict->diag_page_write_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_copy_ok");
    }
    if(!kdbx_chacha20_xor(
           dict->io_page,
           TINFL_PAGED_LZ_DICT_PAGE_SIZE,
           dict->enc_key,
           sizeof(dict->enc_key),
           nonce,
           sizeof(nonce),
           0U)) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_encrypt");
        return false;
    }
    encrypted = true;
    if(dict->diag_page_write_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_encrypt_ok");
    }

    tinfl_file_dict_mac(dict, page_index, dict->io_page, dict->io_mac);
    if(dict->file == NULL) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_open_write");
        goto cleanup;
    }

    if(!tinfl_file_dict_seek_page(dict, page_index, telemetry, "window_write_seek")) {
        goto cleanup;
    }
    if(dict->diag_page_write_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_seek_ok");
    }

    if(storage_file_write(dict->file, dict->io_page, TINFL_PAGED_LZ_DICT_PAGE_SIZE) !=
           TINFL_PAGED_LZ_DICT_PAGE_SIZE ||
       !tinfl_file_dict_runtime_touch(dict, telemetry, "window_write") ||
       storage_file_write(dict->file, dict->io_mac, sizeof(dict->io_mac)) != sizeof(dict->io_mac) ||
       !tinfl_file_dict_runtime_touch(dict, telemetry, "window_write")) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_write");
        goto cleanup;
    }
    dict->page_initialized[page_index] = true;
    ok = true;
    if(dict->diag_page_write_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_io_ok");
    }

    if(dict->diag_page_write_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_ok");
        FURI_LOG_T(TINFL_FILE_TAG, "page_write_ok page=%u", page_index);
    }

cleanup:
    if(encrypted) {
        memset(dict->io_page, 0, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    }
    if(ok && dict->diag_page_write_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_write_restore_ok");
    }

    if(ok) {
        memcpy(dict->io_page, plain, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        dict->io_page_index = page_index;
        dict->io_page_valid = true;
    }

    return ok;
}

static bool tinfl_file_dict_read_page(
    tinfl_file_dict* dict,
    uint16_t page_index,
    uint8_t* out,
    tinfl_paged_telemetry* telemetry) {
    furi_assert(dict);
    furi_assert(out);

    if(!dict->page_initialized[page_index]) {
        memset(out, 0, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        if(dict->diag_page_read_count++ < 16U) {
            tinfl_file_dict_trace_runtime(dict, "page_read_zero");
            FURI_LOG_T(TINFL_FILE_TAG, "page_read_zero page=%u", page_index);
        }
        return true;
    }

    if(dict->diag_page_read_count++ < 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_read_begin");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "page_read_begin page=%u free=%lu max=%lu",
            page_index,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }

    if(dict->io_page_valid && dict->io_page_index == page_index && dict->io_page != out) {
        memcpy(out, dict->io_page, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    } else if(
        !tinfl_file_dict_load_plain_page_to_buffer(
            dict, page_index, out, telemetry, "window_read_seek", "window_read")) {
        return false;
    }

    if(dict->diag_page_read_count <= 16U) {
        tinfl_file_dict_trace_runtime(dict, "page_read_ok");
        FURI_LOG_T(TINFL_FILE_TAG, "page_read_ok page=%u", page_index);
    }
    return true;
}

#if TINFL_FILE_DICT_USE_COPY_MATCH
static uint8_t* tinfl_file_dict_get_source_page(
    tinfl_file_dict* dict,
    uint16_t page_index,
    tinfl_paged_telemetry* telemetry) {
    furi_assert(dict);

    for(size_t index = 0; index < dict->cache_pages; index++) {
        tinfl_file_cache_page* page = &dict->cache[index];
        if(page->valid && page->page_index == page_index) {
            page->age = ++dict->age_counter;
            return page->data;
        }
    }

    if(dict->io_page_valid && dict->io_page_index == page_index) {
        return dict->io_page;
    }

    if(!tinfl_file_dict_load_plain_page_to_buffer(
           dict, page_index, dict->io_page, telemetry, "window_source_seek", "window_source")) {
        return NULL;
    }

    dict->io_page_index = page_index;
    dict->io_page_valid = true;
    return dict->io_page;
}
#endif

#if TINFL_FILE_DICT_USE_COPY_MATCH
static bool tinfl_file_dict_copy_seed_to_page(
    tinfl_file_dict* dict,
    tinfl_file_cache_page* dst_page,
    size_t dst_in_page,
    size_t src_offset,
    size_t len,
    tinfl_paged_telemetry* telemetry) {
    furi_assert(dict);
    furi_assert(dst_page);

    while(len > 0U) {
        const size_t src_masked = src_offset & (TINFL_LZ_DICT_SIZE - 1U);
        const uint16_t src_page_index = (uint16_t)(src_masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        const size_t src_in_page = src_masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t step = MZ_MIN(len, (size_t)TINFL_PAGED_LZ_DICT_PAGE_SIZE - src_in_page);
        const uint8_t* src_page =
            (src_page_index == dst_page->page_index) ? dst_page->data :
                                                       tinfl_file_dict_get_source_page(
                                                           dict, src_page_index, telemetry);
        if(src_page == NULL) {
            return false;
        }

        if(src_page == dst_page->data) {
            memmove(dst_page->data + dst_in_page, src_page + src_in_page, step);
        } else {
            memcpy(dst_page->data + dst_in_page, src_page + src_in_page, step);
        }
        dst_in_page += step;
        src_offset += step;
        len -= step;
    }

    return true;
}

static bool tinfl_file_dict_copy_match_impl(
    void* impl,
    size_t dst_offset,
    size_t src_offset,
    size_t len,
    tinfl_paged_runtime* runtime) {
    tinfl_file_dict* dict = impl;
    const size_t match_distance = (dst_offset - src_offset) & (TINFL_LZ_DICT_SIZE - 1U);

    if(match_distance == 0U) {
        return false;
    }

    while(len > 0U) {
        const size_t dst_masked = dst_offset & (TINFL_LZ_DICT_SIZE - 1U);
        const uint16_t dst_page_index = (uint16_t)(dst_masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        const size_t dst_in_page = dst_masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t chunk = MZ_MIN(len, (size_t)TINFL_PAGED_LZ_DICT_PAGE_SIZE - dst_in_page);
        size_t seed = MZ_MIN(chunk, match_distance);

        if(!tinfl_paged_runtime_heartbeat(runtime)) {
            return false;
        }

        tinfl_file_cache_page* dst_page =
            tinfl_file_dict_acquire_page(dict, dst_page_index, true, dict->telemetry);
        if(dst_page == NULL) {
            return false;
        }

        if(seed > 0U &&
           !tinfl_file_dict_copy_seed_to_page(
               dict, dst_page, dst_in_page, src_offset, seed, dict->telemetry)) {
            return false;
        }

        size_t produced = seed;
        while(produced < chunk) {
            size_t step = chunk - produced;
            if(step > produced) {
                step = produced;
            }
            memmove(
                dst_page->data + dst_in_page + produced,
                dst_page->data + dst_in_page,
                step);
            produced += step;
        }
        dst_page->dirty = true;

        if(dict->io_page_valid && dict->io_page_index == dst_page_index && dict->io_page != dst_page->data) {
            memcpy(dict->io_page + dst_in_page, dst_page->data + dst_in_page, chunk);
        }

        if(!tinfl_paged_runtime_note_output(runtime, chunk)) {
            return false;
        }

        dst_offset += chunk;
        src_offset += chunk;
        len -= chunk;
    }

    return true;
}
#endif

static bool tinfl_file_dict_flush_cache_entry(
    tinfl_file_dict* dict,
    size_t cache_index,
    tinfl_paged_telemetry* telemetry) {
    tinfl_file_cache_page* page = &dict->cache[cache_index];
    if(!page->valid || !page->dirty) {
        return true;
    }

    if(!tinfl_file_dict_write_page(dict, page->page_index, page->data, telemetry)) {
        return false;
    }

    page->dirty = false;
    return true;
}

static tinfl_file_cache_page* tinfl_file_dict_acquire_page(
    tinfl_file_dict* dict,
    uint16_t page_index,
    bool for_write,
    tinfl_paged_telemetry* telemetry) {
    tinfl_file_cache_page* victim = NULL;

    if(dict != NULL && dict->diag_acquire_count++ < 8U) {
        tinfl_file_dict_trace_runtime(dict, "acquire_begin");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "acquire page=%u write=%u cache=%lu free=%lu max=%lu",
            page_index,
            for_write ? 1U : 0U,
            (unsigned long)dict->cache_pages,
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
    }

    for(size_t index = 0; index < dict->cache_pages; index++) {
        tinfl_file_cache_page* page = &dict->cache[index];
        if(page->valid && page->page_index == page_index) {
            if(dict->diag_acquire_count <= 8U) {
                tinfl_file_dict_trace_runtime(dict, "acquire_hit");
                FURI_LOG_T(TINFL_FILE_TAG, "acquire_hit page=%u slot=%lu", page_index, (unsigned long)index);
            }
            page->age = ++dict->age_counter;
            return page;
        }
        const bool victim_invalid = (victim == NULL) || !victim->valid;
        const bool page_invalid = !page->valid;
        const bool victim_clean = (victim != NULL) && victim->valid && !victim->dirty;
        const bool page_clean = page->valid && !page->dirty;
        const bool prefer_page =
            (victim == NULL) ||
            (page_invalid && !victim_invalid) ||
            (!page_invalid && !victim_invalid && page_clean && !victim_clean) ||
            (!page_invalid && !victim_invalid && page->dirty == victim->dirty &&
             page->age < victim->age);
        if(prefer_page) {
            victim = page;
        }
    }

    if(victim == NULL || victim->data == NULL) {
        tinfl_file_dict_note_budget_issue(dict, telemetry, "window_cache");
        return NULL;
    }

    if(dict->diag_acquire_count <= 8U) {
        tinfl_file_dict_trace_runtime(dict, "acquire_miss");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "acquire_miss page=%u victim_valid=%u victim_page=%u victim_dirty=%u",
            page_index,
            victim->valid ? 1U : 0U,
            victim->valid ? victim->page_index : 0U,
            victim->dirty ? 1U : 0U);
    }

    if(victim->valid && victim->dirty &&
       !tinfl_file_dict_flush_cache_entry(dict, (size_t)(victim - dict->cache), telemetry)) {
        return NULL;
    }

    if(!tinfl_file_dict_read_page(dict, page_index, victim->data, telemetry)) {
        return NULL;
    }

    victim->valid = true;
    victim->dirty = for_write;
    victim->page_index = page_index;
    victim->age = ++dict->age_counter;
    return victim;
}

static mz_uint8 tinfl_file_dict_get(const void* impl, size_t offset) {
    tinfl_file_dict* dict = (tinfl_file_dict*)impl;
    const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
    const uint16_t page_index = (uint16_t)(masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    if(dict != NULL && dict->diag_get_count++ < 4U) {
        tinfl_file_dict_trace_runtime(dict, "dict_get");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "dict_get offset=%lu page=%u in=%lu",
            (unsigned long)offset,
            page_index,
            (unsigned long)in_page);
    }
    tinfl_file_cache_page* page =
        tinfl_file_dict_acquire_page(dict, page_index, false, dict->telemetry);
    return page != NULL ? page->data[in_page] : 0U;
}

static bool tinfl_file_dict_failed(const void* impl) {
    const tinfl_file_dict* dict = impl;
    return dict != NULL && dict->fatal_failed;
}

static void tinfl_file_dict_set(void* impl, size_t offset, mz_uint8 value) {
    tinfl_file_dict* dict = impl;
    const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
    const uint16_t page_index = (uint16_t)(masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    if(dict != NULL && dict->diag_set_count++ < 8U) {
        tinfl_file_dict_trace_runtime(dict, "dict_set");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "dict_set offset=%lu page=%u in=%lu value=%u",
            (unsigned long)offset,
            page_index,
            (unsigned long)in_page,
            value);
    }
    tinfl_file_cache_page* page =
        tinfl_file_dict_acquire_page(dict, page_index, true, dict->telemetry);
    if(page != NULL) {
        page->data[in_page] = value;
        page->dirty = true;
        if(dict->io_page_valid && dict->io_page_index == page_index && dict->io_page != page->data) {
            dict->io_page[in_page] = value;
        }
    }
}

static void tinfl_file_dict_write(void* impl, size_t offset, const mz_uint8* src, size_t len) {
    tinfl_file_dict* dict = impl;

    if(dict != NULL && dict->diag_write_count++ < 4U) {
        tinfl_file_dict_trace_runtime(dict, "dict_write");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "dict_write offset=%lu len=%lu",
            (unsigned long)offset,
            (unsigned long)len);
    }

    while(len > 0U) {
        const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
        const uint16_t page_index = (uint16_t)(masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t chunk = MZ_MIN(len, (size_t)TINFL_PAGED_LZ_DICT_PAGE_SIZE - in_page);
        tinfl_file_cache_page* page =
            tinfl_file_dict_acquire_page(dict, page_index, true, dict->telemetry);
        if(page == NULL) {
            return;
        }
        memcpy(page->data + in_page, src, chunk);
        page->dirty = true;
        if(dict->io_page_valid && dict->io_page_index == page_index && dict->io_page != page->data) {
            memcpy(dict->io_page + in_page, src, chunk);
        }
        offset += chunk;
        src += chunk;
        len -= chunk;
    }
}

static int tinfl_file_dict_flush(
    void* impl,
    size_t offset,
    size_t len,
    tinfl_put_buf_func_ptr callback,
    void* user) {
    tinfl_file_dict* dict = impl;

    if(dict != NULL && dict->diag_flush_count++ < 4U) {
        tinfl_file_dict_trace_runtime(dict, "dict_flush");
        FURI_LOG_T(
            TINFL_FILE_TAG,
            "dict_flush offset=%lu len=%lu",
            (unsigned long)offset,
            (unsigned long)len);
    }

    while(len > 0U) {
        const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
        const uint16_t page_index = (uint16_t)(masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t chunk = MZ_MIN(len, (size_t)TINFL_PAGED_LZ_DICT_PAGE_SIZE - in_page);
        tinfl_file_cache_page* page =
            tinfl_file_dict_acquire_page(dict, page_index, false, dict->telemetry);
        if(page == NULL || callback(page->data + in_page, (int)chunk, user) == 0) {
            return 0;
        }
        offset += chunk;
        len -= chunk;
    }

    return 1;
}

static const tinfl_dict_ops tinfl_file_dict_ops = {
    .get = tinfl_file_dict_get,
    .set = tinfl_file_dict_set,
    .write = tinfl_file_dict_write,
    .flush = tinfl_file_dict_flush,
    .copy_match =
#if TINFL_FILE_DICT_USE_COPY_MATCH
        tinfl_file_dict_copy_match_impl,
#else
        NULL,
#endif
    .failed = tinfl_file_dict_failed,
};

static bool tinfl_paged_dict_alloc(tinfl_paged_dict* dict, tinfl_paged_telemetry* telemetry) {
    memset(dict, 0, sizeof(*dict));

    for(size_t index = 0; index < TINFL_PAGED_LZ_DICT_PAGE_COUNT; index++) {
        dict->pages[index] = malloc(TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        if(dict->pages[index] == NULL) {
            if(telemetry != NULL) {
                telemetry->failed_page_index = index;
            }
            for(size_t rollback = 0; rollback < index; rollback++) {
                memzero(dict->pages[rollback], TINFL_PAGED_LZ_DICT_PAGE_SIZE);
                free(dict->pages[rollback]);
                dict->pages[rollback] = NULL;
            }
            return false;
        }

        memset(dict->pages[index], 0, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        if(telemetry != NULL) {
            telemetry->pages_allocated = index + 1U;
        }
    }

    return true;
}

static void tinfl_paged_dict_free(tinfl_paged_dict* dict) {
    if(dict == NULL) {
        return;
    }

    for(size_t index = 0; index < TINFL_PAGED_LZ_DICT_PAGE_COUNT; index++) {
        if(dict->pages[index] != NULL) {
            memzero(dict->pages[index], TINFL_PAGED_LZ_DICT_PAGE_SIZE);
            free(dict->pages[index]);
            dict->pages[index] = NULL;
        }
    }
}

static bool tinfl_file_dict_alloc(
    tinfl_file_dict* dict,
    const tinfl_paged_file_config* config,
    tinfl_paged_telemetry* telemetry) {
    const char* file_path = (config != NULL) ? config->file_path : NULL;
    size_t preferred_cache_pages =
        (config != NULL && config->preferred_cache_pages > 0U) ? config->preferred_cache_pages :
                                                                 TINFL_FILE_DICT_MAX_CACHE_PAGES;
    size_t minimum_cache_pages =
        (config != NULL && config->minimum_cache_pages > 0U) ? config->minimum_cache_pages :
                                                               TINFL_FILE_DICT_MIN_CACHE_PAGES;
    memset(dict, 0, sizeof(*dict));
    dict->telemetry = telemetry;

    if(file_path == NULL || minimum_cache_pages > preferred_cache_pages) {
        tinfl_file_dict_note_budget_issue(dict, telemetry, "window_config");
        return false;
    }

    if(config != NULL && config->storage != NULL) {
        dict->storage = config->storage;
        dict->owns_storage = false;
        tinfl_file_dict_trace_step(dict, telemetry, "window_storage_reuse");
    } else {
        dict->storage = furi_record_open(RECORD_STORAGE);
        dict->owns_storage = true;
        if(dict->storage == NULL) {
            tinfl_file_dict_note_failure(dict, telemetry, "window_storage");
            return false;
        }
        tinfl_file_dict_trace_step(dict, telemetry, "window_storage_open");
    }

    dict->io_page = malloc(TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    if(dict->io_page == NULL) {
        tinfl_file_dict_note_budget_issue(dict, telemetry, "window_io_page_alloc");
        return false;
    }
    memset(dict->io_page, 0, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
    dict->io_page_index = 0U;
    dict->io_page_valid = false;

    for(size_t target = preferred_cache_pages; target >= minimum_cache_pages; target--) {
        bool cache_ok = true;
        tinfl_file_dict_trace_step(dict, telemetry, "window_cache_attempt");
        for(size_t index = 0; index < target; index++) {
            dict->cache[index].data = malloc(TINFL_PAGED_LZ_DICT_PAGE_SIZE);
            if(dict->cache[index].data == NULL) {
                cache_ok = false;
                for(size_t rollback = 0; rollback < index; rollback++) {
                    memzero(dict->cache[rollback].data, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
                    free(dict->cache[rollback].data);
                    dict->cache[rollback].data = NULL;
                }
                break;
            }
            memset(dict->cache[index].data, 0, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        }
        if(cache_ok) {
            dict->cache_pages = target;
            tinfl_file_dict_trace_step(dict, telemetry, "window_cache_ok");
            break;
        }
    }

    if(dict->cache_pages < minimum_cache_pages) {
        tinfl_file_dict_note_budget_issue(dict, telemetry, "window_cache_alloc");
        return false;
    }

    if(telemetry != NULL) {
        telemetry->cache_pages = dict->cache_pages;
        telemetry->pages_allocated = dict->cache_pages;
    }

    dict->file_path = file_path;
    if(!storage_simply_mkdir(dict->storage, EXT_PATH("apps_data/flippass"))) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_mkdir");
        return false;
    }
    tinfl_file_dict_trace_step(dict, telemetry, "window_mkdir_ok");
    if(!tinfl_file_cleanup_file(dict->storage, dict->file_path)) {
        tinfl_file_dict_note_failure(dict, telemetry, "window_cleanup");
        return false;
    }
    tinfl_file_dict_trace_step(dict, telemetry, "window_cleanup_ok");

    dict->file = storage_file_alloc(dict->storage);
    if(dict->file == NULL) {
        tinfl_file_dict_note_budget_issue(dict, telemetry, "window_file_alloc");
        return false;
    }
    tinfl_file_dict_trace_step(dict, telemetry, "window_file_alloc_ok");

    if(!storage_file_open(dict->file, dict->file_path, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(dict->file);
        storage_file_free(dict->file);
        dict->file = NULL;
        tinfl_file_dict_note_failure(dict, telemetry, "window_open_create");
        return false;
    }
    tinfl_file_dict_trace_step(dict, telemetry, "window_open_create_ok");

    furi_hal_random_fill_buf(dict->session_master, sizeof(dict->session_master));
    furi_hal_random_fill_buf(dict->nonce_prefix, sizeof(dict->nonce_prefix));
    tinfl_file_dict_derive_keys(dict);
    tinfl_file_dict_trace_step(dict, telemetry, "window_keys_ok");

    return true;
}

static void tinfl_file_dict_free(tinfl_file_dict* dict) {
    if(dict == NULL) {
        return;
    }

    for(size_t index = 0; index < dict->cache_pages; index++) {
        tinfl_file_cache_page* page = &dict->cache[index];
        if(page->data != NULL) {
            memzero(page->data, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
            free(page->data);
            page->data = NULL;
        }
    }

    if(dict->file != NULL) {
        storage_file_sync(dict->file);
        storage_file_close(dict->file);
        storage_file_free(dict->file);
        dict->file = NULL;
    }

    if(dict->storage != NULL) {
        tinfl_file_cleanup_file(dict->storage, dict->file_path);
        if(dict->owns_storage) {
            furi_record_close(RECORD_STORAGE);
        }
        dict->storage = NULL;
        dict->owns_storage = false;
    }

    memzero(dict->session_master, sizeof(dict->session_master));
    memzero(dict->enc_key, sizeof(dict->enc_key));
    memzero(dict->mac_key, sizeof(dict->mac_key));
    memzero(dict->nonce_prefix, sizeof(dict->nonce_prefix));
    if(dict->io_page != NULL) {
        memzero(dict->io_page, TINFL_PAGED_LZ_DICT_PAGE_SIZE);
        free(dict->io_page);
        dict->io_page = NULL;
    }
    dict->io_page_index = 0U;
    dict->io_page_valid = false;
    memzero(dict->io_mac, sizeof(dict->io_mac));
}

static mz_uint8 tinfl_paged_dict_get(const tinfl_paged_dict* dict, size_t offset) {
    const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
    const size_t page = masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    return dict->pages[page][in_page];
}

static void tinfl_paged_dict_set(tinfl_paged_dict* dict, size_t offset, mz_uint8 value) {
    const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
    const size_t page = masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
    dict->pages[page][in_page] = value;
}

static void tinfl_paged_dict_write(
    tinfl_paged_dict* dict,
    size_t offset,
    const mz_uint8* src,
    size_t len) {
    while(len > 0U) {
        const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
        const size_t page = masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t chunk =
            MZ_MIN(len, (size_t)TINFL_PAGED_LZ_DICT_PAGE_SIZE - in_page);
        memcpy(dict->pages[page] + in_page, src, chunk);
        offset += chunk;
        src += chunk;
        len -= chunk;
    }
}

static int tinfl_paged_dict_flush(
    const tinfl_paged_dict* dict,
    size_t offset,
    size_t len,
    tinfl_put_buf_func_ptr callback,
    void* user) {
    while(len > 0U) {
        const size_t masked = offset & (TINFL_LZ_DICT_SIZE - 1U);
        const size_t page = masked / TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t in_page = masked % TINFL_PAGED_LZ_DICT_PAGE_SIZE;
        const size_t chunk =
            MZ_MIN(len, (size_t)TINFL_PAGED_LZ_DICT_PAGE_SIZE - in_page);
        if(callback(dict->pages[page] + in_page, (int)chunk, user) == 0) {
            return 0;
        }
        offset += chunk;
        len -= chunk;
    }

    return 1;
}

static mz_uint8 tinfl_ram_dict_get(const void* impl, size_t offset) {
    return tinfl_paged_dict_get((const tinfl_paged_dict*)impl, offset);
}

static void tinfl_ram_dict_set(void* impl, size_t offset, mz_uint8 value) {
    tinfl_paged_dict_set((tinfl_paged_dict*)impl, offset, value);
}

static void tinfl_ram_dict_write(void* impl, size_t offset, const mz_uint8* src, size_t len) {
    tinfl_paged_dict_write((tinfl_paged_dict*)impl, offset, src, len);
}

static int tinfl_ram_dict_flush(
    void* impl,
    size_t offset,
    size_t len,
    tinfl_put_buf_func_ptr callback,
    void* user) {
    return tinfl_paged_dict_flush((const tinfl_paged_dict*)impl, offset, len, callback, user);
}

static bool tinfl_ram_dict_failed(const void* impl) {
    UNUSED(impl);
    return false;
}

static const tinfl_dict_ops tinfl_ram_dict_ops = {
    .get = tinfl_ram_dict_get,
    .set = tinfl_ram_dict_set,
    .write = tinfl_ram_dict_write,
    .flush = tinfl_ram_dict_flush,
    .copy_match = NULL,
    .failed = tinfl_ram_dict_failed,
};

static void tinfl_clear_tree(tinfl_decompressor* r) {
    if(r->m_type == 0) {
        MZ_CLEAR_ARR(r->m_tree_0);
    } else if(r->m_type == 1) {
        MZ_CLEAR_ARR(r->m_tree_1);
    } else {
        MZ_CLEAR_ARR(r->m_tree_2);
    }
}

static tinfl_status tinfl_decompress_paged(
    tinfl_decompressor* r,
    const mz_uint8* pIn_buf_next,
    size_t* pIn_buf_size,
    tinfl_dict_view* dict,
    size_t out_buf_next,
    size_t* pOut_buf_size,
    const mz_uint32 decomp_flags,
    tinfl_paged_runtime* runtime) {
    static const mz_uint16 s_length_base[31] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
    static const mz_uint8 s_length_extra[31] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0};
    static const mz_uint16 s_dist_base[32] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193,
        12289, 16385, 24577, 0, 0};
    static const mz_uint8 s_dist_extra[32] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    static const mz_uint8 s_length_dezigzag[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
        11, 4, 12, 3, 13, 2, 14, 1, 15};
    static const mz_uint16 s_min_table_sizes[3] = {257, 1, 4};

    mz_int16* pTrees[3];
    mz_uint8* pCode_sizes[3];
    tinfl_status status = TINFL_STATUS_FAILED;
    mz_uint32 num_bits, dist, counter, num_extra;
    tinfl_bit_buf_t bit_buf;
    const mz_uint8* pIn_buf_cur = pIn_buf_next;
    const mz_uint8* const pIn_buf_end = pIn_buf_next + *pIn_buf_size;
    size_t pOut_buf_cur = out_buf_next;
    const size_t pOut_buf_end = out_buf_next + *pOut_buf_size;
    const size_t out_buf_size_mask = TINFL_LZ_DICT_SIZE - 1U;
    size_t dist_from_out_buf_start;
    size_t paged_copy_len = 0U;

    if(dict == NULL || out_buf_next > TINFL_LZ_DICT_SIZE ||
       *pOut_buf_size > (TINFL_LZ_DICT_SIZE - out_buf_next) ||
       (decomp_flags &
        (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
         TINFL_FLAG_COMPUTE_ADLER32)) != 0U) {
        *pIn_buf_size = 0U;
        *pOut_buf_size = 0U;
        return TINFL_STATUS_BAD_PARAM;
    }

    pTrees[0] = r->m_tree_0;
    pTrees[1] = r->m_tree_1;
    pTrees[2] = r->m_tree_2;
    pCode_sizes[0] = r->m_code_size_0;
    pCode_sizes[1] = r->m_code_size_1;
    pCode_sizes[2] = r->m_code_size_2;

    num_bits = r->m_num_bits;
    bit_buf = r->m_bit_buf;
    dist = r->m_dist;
    counter = r->m_counter;
    num_extra = r->m_num_extra;
    dist_from_out_buf_start = r->m_dist_from_out_buf_start;

    TINFL_CR_BEGIN

    bit_buf = num_bits = dist = counter = num_extra = r->m_zhdr0 = r->m_zhdr1 = 0;
    r->m_z_adler32 = r->m_check_adler32 = 1;

    do {
        TINFL_GET_BITS(3, r->m_final, 3);
        r->m_type = r->m_final >> 1;
        if(r->m_type == 0) {
            TINFL_SKIP_BITS(5, num_bits & 7);
            for(counter = 0; counter < 4; ++counter) {
                if(num_bits) {
                    TINFL_GET_BITS(6, r->m_raw_header[counter], 8);
                } else {
                    TINFL_GET_BYTE(7, r->m_raw_header[counter]);
                }
            }
            if((counter = (r->m_raw_header[0] | (r->m_raw_header[1] << 8))) !=
               (mz_uint)(0xFFFF ^ (r->m_raw_header[2] | (r->m_raw_header[3] << 8)))) {
                TINFL_CR_RETURN_FOREVER(39, TINFL_STATUS_FAILED);
            }
            while((counter) && (num_bits)) {
                TINFL_GET_BITS(51, dist, 8);
                while(pOut_buf_cur >= pOut_buf_end) {
                    TINFL_CR_RETURN(52, TINFL_STATUS_HAS_MORE_OUTPUT);
                }
                dict->ops->set(dict->impl, pOut_buf_cur++, (mz_uint8)dist);
                if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                    TINFL_CR_RETURN_FOREVER(61, TINFL_STATUS_FAILED);
                }
                if(!tinfl_paged_runtime_note_output(runtime, 1U)) {
                    TINFL_CR_RETURN_FOREVER(55, TINFL_STATUS_FAILED);
                }
                counter--;
            }
            while(counter) {
                size_t n;
                while(pOut_buf_cur >= pOut_buf_end) {
                    TINFL_CR_RETURN(9, TINFL_STATUS_HAS_MORE_OUTPUT);
                }
                while(pIn_buf_cur >= pIn_buf_end) {
                    TINFL_CR_RETURN(
                        38,
                        (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ?
                            TINFL_STATUS_NEEDS_MORE_INPUT :
                            TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS);
                }
                n = MZ_MIN(
                    MZ_MIN((size_t)(pOut_buf_end - pOut_buf_cur), (size_t)(pIn_buf_end - pIn_buf_cur)),
                    counter);
                dict->ops->write(dict->impl, pOut_buf_cur, pIn_buf_cur, n);
                if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                    TINFL_CR_RETURN_FOREVER(62, TINFL_STATUS_FAILED);
                }
                pIn_buf_cur += n;
                pOut_buf_cur += n;
                if(!tinfl_paged_runtime_note_output(runtime, n)) {
                    TINFL_CR_RETURN_FOREVER(56, TINFL_STATUS_FAILED);
                }
                counter -= (mz_uint)n;
            }
        } else if(r->m_type == 3) {
            TINFL_CR_RETURN_FOREVER(10, TINFL_STATUS_FAILED);
        } else {
            if(r->m_type == 1) {
                mz_uint8* p = r->m_code_size_0;
                mz_uint i;
                r->m_table_sizes[0] = 288;
                r->m_table_sizes[1] = 32;
                memset(r->m_code_size_1, 5, 32);
                for(i = 0; i <= 143; ++i) {
                    *p++ = 8;
                }
                for(; i <= 255; ++i) {
                    *p++ = 9;
                }
                for(; i <= 279; ++i) {
                    *p++ = 7;
                }
                for(; i <= 287; ++i) {
                    *p++ = 8;
                }
            } else {
                for(counter = 0; counter < 3; ++counter) {
                    TINFL_GET_BITS(11, r->m_table_sizes[counter], "\05\05\04"[counter]);
                    r->m_table_sizes[counter] += s_min_table_sizes[counter];
                }

                MZ_CLEAR_ARR(r->m_code_size_2);
                for(counter = 0; counter < r->m_table_sizes[2]; ++counter) {
                    mz_uint s;
                    TINFL_GET_BITS(14, s, 3);
                    r->m_code_size_2[s_length_dezigzag[counter]] = (mz_uint8)s;
                }
                r->m_table_sizes[2] = 19;
            }

            for(; (int)r->m_type >= 0; r->m_type--) {
                int tree_next, tree_cur;
                mz_int16* pLookUp;
                mz_int16* pTree;
                mz_uint8* pCode_size;
                mz_uint i, j, used_syms, total, sym_index, next_code[17], total_syms[16];

                pLookUp = r->m_look_up[r->m_type];
                pTree = pTrees[r->m_type];
                pCode_size = pCode_sizes[r->m_type];
                MZ_CLEAR_ARR(total_syms);
                memset(pLookUp, 0, sizeof(r->m_look_up[0]));
                tinfl_clear_tree(r);

                for(i = 0; i < r->m_table_sizes[r->m_type]; ++i) {
                    total_syms[pCode_size[i]]++;
                }

                used_syms = 0;
                total = 0;
                next_code[0] = next_code[1] = 0;
                for(i = 1; i <= 15; ++i) {
                    used_syms += total_syms[i];
                    next_code[i + 1] = (total = ((total + total_syms[i]) << 1));
                }

                if((65536 != total) && (used_syms > 1)) {
                    TINFL_CR_RETURN_FOREVER(35, TINFL_STATUS_FAILED);
                }

                for(tree_next = -1, sym_index = 0; sym_index < r->m_table_sizes[r->m_type];
                    ++sym_index) {
                    mz_uint rev_code = 0;
                    mz_uint l;
                    mz_uint cur_code;
                    mz_uint code_size = pCode_size[sym_index];

                    if(!code_size) {
                        continue;
                    }

                    cur_code = next_code[code_size]++;
                    for(l = code_size; l > 0; l--, cur_code >>= 1) {
                        rev_code = (rev_code << 1) | (cur_code & 1U);
                    }

                    if(code_size <= TINFL_FAST_LOOKUP_BITS) {
                        mz_int16 k = (mz_int16)((code_size << 9) | sym_index);
                        while(rev_code < TINFL_FAST_LOOKUP_SIZE) {
                            pLookUp[rev_code] = k;
                            rev_code += (1U << code_size);
                        }
                        continue;
                    }

                    if(0 == (tree_cur = pLookUp[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)])) {
                        pLookUp[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)] = (mz_int16)tree_next;
                        tree_cur = tree_next;
                        tree_next -= 2;
                    }

                    rev_code >>= (TINFL_FAST_LOOKUP_BITS - 1);
                    for(j = code_size; j > (TINFL_FAST_LOOKUP_BITS + 1); j--) {
                        tree_cur -= ((rev_code >>= 1) & 1U);
                        if(!pTree[-tree_cur - 1]) {
                            pTree[-tree_cur - 1] = (mz_int16)tree_next;
                            tree_cur = tree_next;
                            tree_next -= 2;
                        } else {
                            tree_cur = pTree[-tree_cur - 1];
                        }
                    }

                    tree_cur -= ((rev_code >>= 1) & 1U);
                    pTree[-tree_cur - 1] = (mz_int16)sym_index;
                }

                if(r->m_type == 2) {
                    for(counter = 0; counter < (r->m_table_sizes[0] + r->m_table_sizes[1]);) {
                        mz_uint s;
                        TINFL_HUFF_DECODE(16, dist, r->m_look_up[2], r->m_tree_2);
                        if(dist < 16) {
                            r->m_len_codes[counter++] = (mz_uint8)dist;
                            continue;
                        }
                        if((dist == 16) && (!counter)) {
                            TINFL_CR_RETURN_FOREVER(17, TINFL_STATUS_FAILED);
                        }
                        num_extra = "\02\03\07"[dist - 16];
                        TINFL_GET_BITS(18, s, num_extra);
                        s += "\03\03\013"[dist - 16];
                        memset(
                            r->m_len_codes + counter,
                            (dist == 16) ? r->m_len_codes[counter - 1] : 0,
                            s);
                        counter += s;
                    }

                    if((r->m_table_sizes[0] + r->m_table_sizes[1]) != counter) {
                        TINFL_CR_RETURN_FOREVER(21, TINFL_STATUS_FAILED);
                    }

                    memcpy(r->m_code_size_0, r->m_len_codes, r->m_table_sizes[0]);
                    memcpy(
                        r->m_code_size_1,
                        r->m_len_codes + r->m_table_sizes[0],
                        r->m_table_sizes[1]);
                }
            }

            for(;;) {
                if(!tinfl_paged_runtime_heartbeat(runtime)) {
                    TINFL_CR_RETURN_FOREVER(68, TINFL_STATUS_FAILED);
                }
                for(;;) {
                    if(((pIn_buf_end - pIn_buf_cur) < 4) || ((pOut_buf_end - pOut_buf_cur) < 2)) {
                        TINFL_HUFF_DECODE(23, counter, r->m_look_up[0], r->m_tree_0);
                        if(counter >= 256) {
                            break;
                        }
                        while(pOut_buf_cur >= pOut_buf_end) {
                            TINFL_CR_RETURN(24, TINFL_STATUS_HAS_MORE_OUTPUT);
                        }
                        dict->ops->set(dict->impl, pOut_buf_cur++, (mz_uint8)counter);
                        if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                            TINFL_CR_RETURN_FOREVER(63, TINFL_STATUS_FAILED);
                        }
                        if(!tinfl_paged_runtime_note_output(runtime, 1U)) {
                            TINFL_CR_RETURN_FOREVER(57, TINFL_STATUS_FAILED);
                        }
                    } else {
                        int sym2;
                        mz_uint code_len;
#if TINFL_USE_64BIT_BITBUF
                        if(num_bits < 30) {
                            bit_buf |= (((tinfl_bit_buf_t)MZ_READ_LE32(pIn_buf_cur)) << num_bits);
                            pIn_buf_cur += 4;
                            num_bits += 32;
                        }
#else
                        if(num_bits < 15) {
                            bit_buf |= (((tinfl_bit_buf_t)MZ_READ_LE16(pIn_buf_cur)) << num_bits);
                            pIn_buf_cur += 2;
                            num_bits += 16;
                        }
#endif
                        if((sym2 = r->m_look_up[0][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) {
                            code_len = sym2 >> 9;
                        } else {
                            code_len = TINFL_FAST_LOOKUP_BITS;
                            do {
                                sym2 = r->m_tree_0[~sym2 + ((bit_buf >> code_len++) & 1)];
                            } while(sym2 < 0);
                        }
                        counter = sym2;
                        bit_buf >>= code_len;
                        num_bits -= code_len;
                        if(code_len == 0) {
                            TINFL_CR_RETURN_FOREVER(40, TINFL_STATUS_FAILED);
                        }
                        if(counter & 256) {
                            break;
                        }

#if !TINFL_USE_64BIT_BITBUF
                        if(num_bits < 15) {
                            bit_buf |= (((tinfl_bit_buf_t)MZ_READ_LE16(pIn_buf_cur)) << num_bits);
                            pIn_buf_cur += 2;
                            num_bits += 16;
                        }
#endif
                        if((sym2 = r->m_look_up[0][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) {
                            code_len = sym2 >> 9;
                        } else {
                            code_len = TINFL_FAST_LOOKUP_BITS;
                            do {
                                sym2 = r->m_tree_0[~sym2 + ((bit_buf >> code_len++) & 1)];
                            } while(sym2 < 0);
                        }
                        bit_buf >>= code_len;
                        num_bits -= code_len;
                        if(code_len == 0) {
                            TINFL_CR_RETURN_FOREVER(54, TINFL_STATUS_FAILED);
                        }

                        dict->ops->set(dict->impl, pOut_buf_cur, (mz_uint8)counter);
                        if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                            TINFL_CR_RETURN_FOREVER(64, TINFL_STATUS_FAILED);
                        }
                        if(!tinfl_paged_runtime_note_output(runtime, 1U)) {
                            TINFL_CR_RETURN_FOREVER(58, TINFL_STATUS_FAILED);
                        }
                        if(sym2 & 256) {
                            pOut_buf_cur++;
                            counter = sym2;
                            break;
                        }
                        dict->ops->set(dict->impl, pOut_buf_cur + 1U, (mz_uint8)sym2);
                        if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                            TINFL_CR_RETURN_FOREVER(65, TINFL_STATUS_FAILED);
                        }
                        pOut_buf_cur += 2U;
                        if(!tinfl_paged_runtime_note_output(runtime, 1U)) {
                            TINFL_CR_RETURN_FOREVER(59, TINFL_STATUS_FAILED);
                        }
                    }
                }
                if((counter &= 511) == 256) {
                    break;
                }

                num_extra = s_length_extra[counter - 257];
                counter = s_length_base[counter - 257];
                if(num_extra) {
                    mz_uint extra_bits;
                    TINFL_GET_BITS(25, extra_bits, num_extra);
                    counter += extra_bits;
                }

                TINFL_HUFF_DECODE(26, dist, r->m_look_up[1], r->m_tree_1);
                num_extra = s_dist_extra[dist];
                dist = s_dist_base[dist];
                if(num_extra) {
                    mz_uint extra_bits;
                    TINFL_GET_BITS(27, extra_bits, num_extra);
                    dist += extra_bits;
                }

                dist_from_out_buf_start = pOut_buf_cur;
                while(counter > 0U) {
                    if(dict->ops->copy_match != NULL) {
                        paged_copy_len = counter;
                        if(pOut_buf_cur >= pOut_buf_end) {
                            TINFL_CR_RETURN(73, TINFL_STATUS_HAS_MORE_OUTPUT);
                        }
                        if(paged_copy_len > (pOut_buf_end - pOut_buf_cur)) {
                            paged_copy_len = pOut_buf_end - pOut_buf_cur;
                        }
                        if(!dict->ops->copy_match(
                               dict->impl,
                               pOut_buf_cur,
                               (dist_from_out_buf_start - dist) & out_buf_size_mask,
                               paged_copy_len,
                               runtime)) {
                            TINFL_CR_RETURN_FOREVER(70, TINFL_STATUS_FAILED);
                        }
                        if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                            TINFL_CR_RETURN_FOREVER(72, TINFL_STATUS_FAILED);
                        }
                        pOut_buf_cur += paged_copy_len;
                        dist_from_out_buf_start += paged_copy_len;
                        counter -= paged_copy_len;
                        continue;
                    }

                    counter--;
                    if(!tinfl_paged_runtime_heartbeat(runtime)) {
                        TINFL_CR_RETURN_FOREVER(69, TINFL_STATUS_FAILED);
                    }
                    while(pOut_buf_cur >= pOut_buf_end) {
                        TINFL_CR_RETURN(53, TINFL_STATUS_HAS_MORE_OUTPUT);
                    }
                    mz_uint8 dist_byte =
                        dict->ops->get(
                            dict->impl,
                            (dist_from_out_buf_start++ - dist) & out_buf_size_mask);
                    if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                        TINFL_CR_RETURN_FOREVER(66, TINFL_STATUS_FAILED);
                    }
                    dict->ops->set(
                        dict->impl,
                        pOut_buf_cur++,
                        dist_byte);
                    if(dict->ops->failed != NULL && dict->ops->failed(dict->impl)) {
                        TINFL_CR_RETURN_FOREVER(67, TINFL_STATUS_FAILED);
                    }
                    if(!tinfl_paged_runtime_note_output(runtime, 1U)) {
                        TINFL_CR_RETURN_FOREVER(60, TINFL_STATUS_FAILED);
                    }
                }
            }
        }
    } while(!(r->m_final & 1));

    TINFL_SKIP_BITS(32, num_bits & 7);
    while((pIn_buf_cur > pIn_buf_next) && (num_bits >= 8)) {
        --pIn_buf_cur;
        num_bits -= 8;
    }
    bit_buf &= ~(~(tinfl_bit_buf_t)0 << num_bits);
    TINFL_CR_RETURN_FOREVER(34, TINFL_STATUS_DONE);

    TINFL_CR_FINISH

common_exit:
    if((status != TINFL_STATUS_NEEDS_MORE_INPUT) &&
       (status != TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS)) {
        while((pIn_buf_cur > pIn_buf_next) && (num_bits >= 8)) {
            --pIn_buf_cur;
            num_bits -= 8;
        }
    }

    r->m_num_bits = num_bits;
    r->m_bit_buf = bit_buf & ~(~(tinfl_bit_buf_t)0 << num_bits);
    r->m_dist = dist;
    r->m_counter = counter;
    r->m_num_extra = num_extra;
    r->m_dist_from_out_buf_start = dist_from_out_buf_start;
    *pIn_buf_size = (size_t)(pIn_buf_cur - pIn_buf_next);
    *pOut_buf_size = pOut_buf_cur - out_buf_next;
    return status;
}

int tinfl_decompress_mem_to_callback_paged_ex(
    const void* pIn_buf,
    size_t* pIn_buf_size,
    tinfl_put_buf_func_ptr pPut_buf_func,
    void* pPut_buf_user,
    int flags,
    tinfl_paged_telemetry* pTelemetry) {
    int result = 0;
    tinfl_decompressor* decomp = NULL;
    tinfl_paged_dict dict;
    tinfl_dict_view dict_view;
    tinfl_paged_runtime runtime;
    size_t in_buf_ofs = 0U;
    size_t dict_ofs = 0U;

    tinfl_paged_telemetry_reset(pTelemetry);
    memset(&runtime, 0, sizeof(runtime));
    runtime.telemetry = pTelemetry;
    runtime.deadline_tick = furi_get_tick() + furi_ms_to_ticks(TINFL_PAGED_TIMEOUT_MS);
    runtime.next_trace_output =
        (pTelemetry != NULL && pTelemetry->trace_interval_bytes > 0U) ? pTelemetry->trace_interval_bytes :
                                                                        0U;

    if(pIn_buf == NULL || pIn_buf_size == NULL || pPut_buf_func == NULL ||
        (flags &
         (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
          TINFL_FLAG_COMPUTE_ADLER32)) != 0U) {
        return 0;
    }

    if(!tinfl_paged_dict_alloc(&dict, pTelemetry)) {
        tinfl_paged_trace(&runtime, "alloc_failed");
        return 0;
    }

    decomp = malloc(sizeof(*decomp));
    if(decomp == NULL) {
        tinfl_paged_trace(&runtime, "decomp_alloc_failed");
        tinfl_paged_dict_free(&dict);
        return 0;
    }

    tinfl_paged_trace(&runtime, "alloc_ok");
    dict_view.ops = &tinfl_ram_dict_ops;
    dict_view.impl = &dict;
    tinfl_init(decomp);
    tinfl_paged_trace(&runtime, "begin");

    for(;;) {
        size_t in_buf_size = *pIn_buf_size - in_buf_ofs;
        size_t dst_buf_size = TINFL_LZ_DICT_SIZE - dict_ofs;
        if(pTelemetry != NULL) {
            pTelemetry->loop_count++;
            pTelemetry->last_dict_offset = dict_ofs;
        }
        if(pTelemetry != NULL && pTelemetry->loop_count == 1U) {
            tinfl_paged_trace(&runtime, "first_call");
        }
        const tinfl_status status = tinfl_decompress_paged(
            decomp,
            (const mz_uint8*)pIn_buf + in_buf_ofs,
            &in_buf_size,
            &dict_view,
            dict_ofs,
            &dst_buf_size,
            (mz_uint32)(flags & ~(TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)),
            &runtime);

        if(pTelemetry != NULL) {
            pTelemetry->last_status = status;
            pTelemetry->last_input_advance = in_buf_size;
            pTelemetry->last_output_advance = dst_buf_size;
        }

        in_buf_ofs += in_buf_size;
        if(pTelemetry != NULL) {
            pTelemetry->input_offset = in_buf_ofs;
        }

        if(runtime.telemetry != NULL && runtime.telemetry->timed_out) {
            tinfl_paged_trace(&runtime, "timeout");
            break;
        }

        if(in_buf_size == 0U && dst_buf_size == 0U) {
            if(pTelemetry != NULL) {
                pTelemetry->no_progress_count++;
            }
            tinfl_paged_trace(&runtime, "no_progress");
            break;
        }

        if(dst_buf_size > 0U &&
           !dict_view.ops->flush(
               dict_view.impl, dict_ofs, dst_buf_size, pPut_buf_func, pPut_buf_user)) {
            tinfl_paged_trace(&runtime, "flush_rejected");
            break;
        }
        if(dst_buf_size > 0U && pTelemetry != NULL) {
            pTelemetry->flush_count++;
            tinfl_paged_trace(&runtime, "flush");
        }

        if(status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            result = (status == TINFL_STATUS_DONE);
            tinfl_paged_trace(&runtime, result ? "done" : "inflate_failed");
            break;
        }

        dict_ofs = (dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1U);
    }

    *pIn_buf_size = in_buf_ofs;
    tinfl_paged_dict_free(&dict);
    free(decomp);
    return result;
}

int tinfl_decompress_reader_to_callback_paged_ex(
    tinfl_get_buf_func_ptr pGet_buf_func,
    void* pGet_buf_user,
    size_t* pIn_buf_size,
    tinfl_put_buf_func_ptr pPut_buf_func,
    void* pPut_buf_user,
    int flags,
    tinfl_paged_telemetry* pTelemetry) {
    int result = 0;
    tinfl_decompressor* decomp = NULL;
    tinfl_paged_dict dict;
    tinfl_dict_view dict_view;
    tinfl_paged_runtime runtime;
    uint8_t input_buf[TINFL_PAGED_INPUT_CHUNK_BYTES];
    size_t input_len = 0U;
    size_t input_ofs = 0U;
    size_t total_input = 0U;
    size_t dict_ofs = 0U;
    bool input_eof = false;

    tinfl_paged_telemetry_reset(pTelemetry);
    memset(&runtime, 0, sizeof(runtime));
    runtime.telemetry = pTelemetry;
    runtime.deadline_tick = furi_get_tick() + furi_ms_to_ticks(TINFL_PAGED_TIMEOUT_MS);
    runtime.next_trace_output =
        (pTelemetry != NULL && pTelemetry->trace_interval_bytes > 0U) ? pTelemetry->trace_interval_bytes :
                                                                        0U;

    if(pGet_buf_func == NULL || pIn_buf_size == NULL || pPut_buf_func == NULL ||
       (flags &
        (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
         TINFL_FLAG_COMPUTE_ADLER32)) != 0U) {
        return 0;
    }

    if(!tinfl_paged_dict_alloc(&dict, pTelemetry)) {
        tinfl_paged_trace(&runtime, "alloc_failed");
        return 0;
    }

    decomp = malloc(sizeof(*decomp));
    if(decomp == NULL) {
        tinfl_paged_trace(&runtime, "decomp_alloc_failed");
        tinfl_paged_dict_free(&dict);
        return 0;
    }

    tinfl_paged_trace(&runtime, "alloc_ok");
    dict_view.ops = &tinfl_ram_dict_ops;
    dict_view.impl = &dict;
    tinfl_init(decomp);
    tinfl_paged_trace(&runtime, "begin");

    for(;;) {
        if(input_ofs >= input_len && !input_eof) {
            FURI_LOG_T(
                TINFL_FILE_TAG,
                "file paged input request size=%u",
                (unsigned)sizeof(input_buf));
            input_len = pGet_buf_func(input_buf, sizeof(input_buf), pGet_buf_user);
            input_ofs = 0U;
            if(input_len == 0U) {
                input_eof = true;
            }
        }

        size_t in_buf_size = input_len - input_ofs;
        size_t dst_buf_size = TINFL_LZ_DICT_SIZE - dict_ofs;
        if(pTelemetry != NULL) {
            pTelemetry->loop_count++;
            pTelemetry->last_dict_offset = dict_ofs;
        }
        if(pTelemetry != NULL && pTelemetry->loop_count == 1U) {
            tinfl_paged_trace(&runtime, "first_call");
        }

        const tinfl_status status = tinfl_decompress_paged(
            decomp,
            input_buf + input_ofs,
            &in_buf_size,
            &dict_view,
            dict_ofs,
            &dst_buf_size,
            (mz_uint32)((flags &
                         ~(TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) |
                        (!input_eof ? TINFL_FLAG_HAS_MORE_INPUT : 0U)),
            &runtime);

        if(pTelemetry != NULL) {
            pTelemetry->last_status = status;
            pTelemetry->last_input_advance = in_buf_size;
            pTelemetry->last_output_advance = dst_buf_size;
        }

        input_ofs += in_buf_size;
        total_input += in_buf_size;
        if(pTelemetry != NULL) {
            pTelemetry->input_offset = total_input;
        }

        if(runtime.telemetry != NULL && runtime.telemetry->timed_out) {
            tinfl_paged_trace(&runtime, "timeout");
            break;
        }

        if(in_buf_size == 0U && dst_buf_size == 0U) {
            if(pTelemetry != NULL) {
                pTelemetry->no_progress_count++;
            }
            tinfl_paged_trace(&runtime, "no_progress");
            break;
        }

        if(dst_buf_size > 0U &&
           !dict_view.ops->flush(
               dict_view.impl, dict_ofs, dst_buf_size, pPut_buf_func, pPut_buf_user)) {
            tinfl_paged_trace(&runtime, "flush_rejected");
            break;
        }
        if(dst_buf_size > 0U && pTelemetry != NULL) {
            pTelemetry->flush_count++;
            tinfl_paged_trace(&runtime, "flush");
        }

        if(dst_buf_size > 0U) {
            dict_ofs = (dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1U);
        }

        if(status == TINFL_STATUS_NEEDS_MORE_INPUT) {
            if(input_eof) {
                tinfl_paged_trace(&runtime, "inflate_failed");
                break;
            }
            if(input_ofs >= input_len) {
                input_ofs = 0U;
                input_len = 0U;
            }
            continue;
        }

        if(status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            result = (status == TINFL_STATUS_DONE);
            tinfl_paged_trace(&runtime, result ? "done" : "inflate_failed");
            break;
        }

        if(input_ofs >= input_len) {
            input_ofs = 0U;
            input_len = 0U;
        }
    }

    *pIn_buf_size = total_input;
    memzero(input_buf, sizeof(input_buf));
    tinfl_paged_dict_free(&dict);
    free(decomp);
    return result;
}

int tinfl_decompress_reader_to_callback_file_paged_ex(
    tinfl_get_buf_func_ptr pGet_buf_func,
    void* pGet_buf_user,
    size_t* pIn_buf_size,
    tinfl_put_buf_func_ptr pPut_buf_func,
    void* pPut_buf_user,
    int flags,
    const tinfl_paged_file_config* pFile_config,
    tinfl_decompressor* pDecomp_workspace,
    tinfl_paged_telemetry* pTelemetry) {
    int result = 0;
    const uint32_t stack_space = furi_thread_get_stack_space(furi_thread_get_current_id());
    const size_t decomp_size = sizeof(tinfl_decompressor);
    tinfl_decompressor* decomp = pDecomp_workspace;
    bool decomp_on_stack = false;
    bool decomp_external = pDecomp_workspace != NULL;
    tinfl_file_dict* dict = NULL;
    tinfl_dict_view dict_view;
    tinfl_paged_runtime runtime;
    uint8_t* input_buf = NULL;
    size_t input_len = 0U;
    size_t input_ofs = 0U;
    size_t total_input = 0U;
    size_t dict_ofs = 0U;
    bool input_eof = false;

    tinfl_paged_telemetry_reset(pTelemetry);
    memset(&runtime, 0, sizeof(runtime));
    runtime.telemetry = pTelemetry;
    runtime.deadline_tick = furi_get_tick() + furi_ms_to_ticks(TINFL_PAGED_TIMEOUT_MS);
    runtime.next_trace_output =
        (pTelemetry != NULL && pTelemetry->trace_interval_bytes > 0U) ? pTelemetry->trace_interval_bytes :
                                                                        0U;

    if(pGet_buf_func == NULL || pIn_buf_size == NULL || pPut_buf_func == NULL || pFile_config == NULL ||
       (flags &
        (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
         TINFL_FLAG_COMPUTE_ADLER32)) != 0U) {
        tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_bad_param");
        TINFL_DEBUG_LOG("file_paged bad_param");
        return 0;
    }

    tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_begin");
    TINFL_DEBUG_LOG(
        "file_paged begin stack=%lu free=%lu max=%lu decomp=%lu dict=%lu",
        (unsigned long)stack_space,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block(),
        (unsigned long)decomp_size,
        (unsigned long)sizeof(tinfl_file_dict));

    if(decomp_external) {
        memset(decomp, 0, sizeof(*decomp));
    } else if(stack_space >= (decomp_size + 512U)) {
        decomp = TINFL_RUNTIME_ALLOCA(sizeof(tinfl_decompressor));
        if(decomp != NULL) {
            memset(decomp, 0, sizeof(*decomp));
            decomp_on_stack = true;
        }
    } else if(memmgr_heap_get_max_free_block() >= decomp_size) {
        decomp = malloc(decomp_size);
        if(decomp != NULL) {
            memset(decomp, 0, decomp_size);
        }
    }

    if(decomp == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "file_decomp_alloc");
        tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_decomp_fail");
        TINFL_DEBUG_LOG(
            "file_paged decomp_fail free=%lu max=%lu",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        return 0;
    }
    tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_decomp_ok");
    TINFL_DEBUG_LOG(
        "file_paged decomp_ok stack=%u external=%u free=%lu max=%lu",
        decomp_on_stack ? 1U : 0U,
        decomp_external ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    tinfl_paged_trace(
        &runtime,
        decomp_external ? "file_decomp_external" :
                          (decomp_on_stack ? "file_decomp_stack" : "file_decomp_heap"));

    input_buf = malloc(TINFL_PAGED_INPUT_CHUNK_BYTES);
    if(input_buf == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "window_input_alloc");
        memzero(decomp, sizeof(*decomp));
        if(!decomp_external) {
            free(decomp);
        }
        tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_input_fail");
        TINFL_DEBUG_LOG(
            "file_paged input_fail free=%lu max=%lu",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        return 0;
    }
    memset(input_buf, 0, TINFL_PAGED_INPUT_CHUNK_BYTES);
    tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_input_ok");
    TINFL_DEBUG_LOG(
        "file_paged input_ok free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    tinfl_paged_trace(&runtime, "file_dict_attempt");
    if(memmgr_heap_get_max_free_block() >= sizeof(tinfl_file_dict)) {
        dict = malloc(sizeof(tinfl_file_dict));
        if(dict != NULL) {
            memset(dict, 0, sizeof(*dict));
        }
    }
    if(dict == NULL) {
        tinfl_paged_trace(&runtime, "file_dict_alloc_fail");
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "window_dict_alloc");
        memzero(input_buf, TINFL_PAGED_INPUT_CHUNK_BYTES);
        free(input_buf);
        memzero(decomp, sizeof(*decomp));
        if(!decomp_external && !decomp_on_stack) {
            free(decomp);
        }
        tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_dict_fail");
        TINFL_DEBUG_LOG(
            "file_paged dict_fail free=%lu max=%lu",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        return 0;
    }
    tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_dict_ok");
    TINFL_DEBUG_LOG(
        "file_paged dict_ok free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    tinfl_paged_trace(&runtime, "file_dict_heap");
    tinfl_paged_trace(&runtime, "file_dict_config_begin");
    if(!tinfl_file_dict_alloc(dict, pFile_config, pTelemetry)) {
        tinfl_paged_trace(&runtime, "file_alloc_failed");
        tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_window_fail");
        TINFL_DEBUG_LOG(
            "file_paged window_fail stage=%s free=%lu max=%lu",
            (pTelemetry != NULL && pTelemetry->storage_stage != NULL) ? pTelemetry->storage_stage :
                                                                        "-",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned long)memmgr_heap_get_max_free_block());
        tinfl_file_dict_free(dict);
        free(dict);
        memzero(input_buf, TINFL_PAGED_INPUT_CHUNK_BYTES);
        free(input_buf);
        memzero(decomp, sizeof(*decomp));
        if(!decomp_external && !decomp_on_stack) {
            free(decomp);
        }
        return 0;
    }

    tinfl_paged_trace(&runtime, "file_alloc_ok");
    tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_window_ok");
    TINFL_DEBUG_LOG(
        "file_paged window_ok free=%lu max=%lu",
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());
    dict_view.ops = &tinfl_file_dict_ops;
    dict_view.impl = dict;
    dict->runtime = &runtime;
    tinfl_init(decomp);
    tinfl_paged_trace(&runtime, "file_begin");
    TINFL_DEBUG_LOG(
        "file begin input_len=%lu eof=%u free=%lu max=%lu",
        (unsigned long)input_len,
        input_eof ? 1U : 0U,
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)memmgr_heap_get_max_free_block());

    for(;;) {
        if(input_ofs >= input_len && !input_eof) {
            tinfl_paged_trace(&runtime, "file_input_request");
            if(total_input == 0U) {
                tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_input_request");
                TINFL_DEBUG_LOG(
                    "file input request free=%lu max=%lu",
                    (unsigned long)memmgr_get_free_heap(),
                    (unsigned long)memmgr_heap_get_max_free_block());
            }
            input_len = pGet_buf_func(input_buf, TINFL_PAGED_INPUT_CHUNK_BYTES, pGet_buf_user);
            input_ofs = 0U;
            if(input_len == 0U) {
                input_eof = true;
            }
            tinfl_paged_trace(&runtime, input_len > 0U ? "file_input_ready" : "file_input_eof");
            if(total_input == 0U) {
                tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_input_ready");
                TINFL_DEBUG_LOG(
                    "file input ready len=%lu eof=%u free=%lu max=%lu",
                    (unsigned long)input_len,
                    input_eof ? 1U : 0U,
                    (unsigned long)memmgr_get_free_heap(),
                    (unsigned long)memmgr_heap_get_max_free_block());
            }
        }

        size_t in_buf_size = input_len - input_ofs;
        size_t dst_buf_size = TINFL_LZ_DICT_SIZE - dict_ofs;
        if(pTelemetry != NULL) {
            pTelemetry->loop_count++;
            pTelemetry->last_dict_offset = dict_ofs;
        }
        if(pTelemetry != NULL && pTelemetry->loop_count == 1U) {
            tinfl_paged_trace(&runtime, "file_first_call");
        }
        if(pTelemetry != NULL &&
           (pTelemetry->loop_count <= 4U || (pTelemetry->loop_count % 8U) == 0U)) {
            tinfl_paged_trace(&runtime, "file_call_begin");
        }
        if(pTelemetry != NULL && pTelemetry->loop_count == 1U) {
            tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_call_begin");
            TINFL_DEBUG_LOG(
                "file call begin in=%lu dict_ofs=%lu free=%lu max=%lu",
                (unsigned long)in_buf_size,
                (unsigned long)dict_ofs,
                (unsigned long)memmgr_get_free_heap(),
                (unsigned long)memmgr_heap_get_max_free_block());
        }

        const tinfl_status status = tinfl_decompress_paged(
            decomp,
            input_buf + input_ofs,
            &in_buf_size,
            &dict_view,
            dict_ofs,
            &dst_buf_size,
            (mz_uint32)((flags &
                         ~(TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) |
                        (!input_eof ? TINFL_FLAG_HAS_MORE_INPUT : 0U)),
            &runtime);

        if(pTelemetry != NULL) {
            pTelemetry->last_status = status;
            pTelemetry->last_input_advance = in_buf_size;
            pTelemetry->last_output_advance = dst_buf_size;
        }
        if(pTelemetry != NULL &&
           (pTelemetry->loop_count <= 4U || (pTelemetry->loop_count % 8U) == 0U)) {
            tinfl_paged_trace(&runtime, "file_call_return");
        }
        if(pTelemetry != NULL && pTelemetry->loop_count == 1U) {
            tinfl_debug_trace_telemetry(pTelemetry, "debug_file_paged_call_return");
            TINFL_DEBUG_LOG(
                "file call return status=%d in=%lu out=%lu free=%lu max=%lu",
                status,
                (unsigned long)in_buf_size,
                (unsigned long)dst_buf_size,
                (unsigned long)memmgr_get_free_heap(),
                (unsigned long)memmgr_heap_get_max_free_block());
        }

        input_ofs += in_buf_size;
        total_input += in_buf_size;
        if(pTelemetry != NULL) {
            pTelemetry->input_offset = total_input;
        }

        if(runtime.telemetry != NULL && runtime.telemetry->timed_out) {
            tinfl_paged_trace(&runtime, "timeout");
            break;
        }

        if(dict->storage_failed) {
            tinfl_paged_trace(&runtime, "file_storage_failed");
            break;
        }

        if(in_buf_size == 0U && dst_buf_size == 0U) {
            if(pTelemetry != NULL) {
                pTelemetry->no_progress_count++;
            }
            tinfl_paged_trace(&runtime, "no_progress");
            break;
        }

        if(dst_buf_size > 0U &&
           !dict_view.ops->flush(
               dict_view.impl, dict_ofs, dst_buf_size, pPut_buf_func, pPut_buf_user)) {
            tinfl_paged_trace(&runtime, "flush_rejected");
            break;
        }
        if(dst_buf_size > 0U && pTelemetry != NULL) {
            pTelemetry->flush_count++;
            tinfl_paged_trace(&runtime, "flush");
        }

        if(dst_buf_size > 0U) {
            dict_ofs = (dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1U);
        }

        if(status == TINFL_STATUS_NEEDS_MORE_INPUT) {
            if(input_eof) {
                tinfl_paged_trace(&runtime, "inflate_failed");
                break;
            }
            if(input_ofs >= input_len) {
                input_ofs = 0U;
                input_len = 0U;
            }
            continue;
        }

        if(status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            result = (status == TINFL_STATUS_DONE) && !dict->storage_failed;
            tinfl_paged_trace(&runtime, result ? "done" : "inflate_failed");
            break;
        }

        if(input_ofs >= input_len) {
            input_ofs = 0U;
            input_len = 0U;
        }
    }

    *pIn_buf_size = total_input;
    memzero(input_buf, TINFL_PAGED_INPUT_CHUNK_BYTES);
    free(input_buf);
    tinfl_file_dict_free(dict);
    free(dict);
    memzero(decomp, sizeof(*decomp));
    if(!decomp_external && !decomp_on_stack) {
        free(decomp);
    }
    return result;
}

int tinfl_decompress_mem_to_callback_file_paged_ex(
    const void* pIn_buf,
    size_t* pIn_buf_size,
    tinfl_put_buf_func_ptr pPut_buf_func,
    void* pPut_buf_user,
    int flags,
    const tinfl_paged_file_config* pFile_config,
    tinfl_decompressor* pDecomp_workspace,
    tinfl_paged_telemetry* pTelemetry) {
    int result = 0;
    const uint32_t stack_space = furi_thread_get_stack_space(furi_thread_get_current_id());
    const uint32_t stack_budget = 2048U;
    const size_t decomp_size = sizeof(tinfl_decompressor);
    const uint32_t stack_decomp_margin = 1024U;
    tinfl_decompressor* decomp = pDecomp_workspace;
    bool decomp_on_stack = false;
    bool decomp_external = pDecomp_workspace != NULL;
    tinfl_file_dict* dict = NULL;
    const size_t dict_size = sizeof(tinfl_file_dict);
    const uint32_t stack_dict_margin = 2048U;
    bool dict_on_stack = false;
    tinfl_dict_view dict_view;
    tinfl_paged_runtime runtime;
    size_t in_buf_ofs = 0U;
    size_t dict_ofs = 0U;

    tinfl_paged_telemetry_reset(pTelemetry);
    memset(&runtime, 0, sizeof(runtime));
    runtime.telemetry = pTelemetry;
    runtime.deadline_tick = furi_get_tick() + furi_ms_to_ticks(TINFL_PAGED_TIMEOUT_MS);
    runtime.next_trace_output =
        (pTelemetry != NULL && pTelemetry->trace_interval_bytes > 0U) ? pTelemetry->trace_interval_bytes :
                                                                        0U;

    if(pIn_buf == NULL || pIn_buf_size == NULL || pPut_buf_func == NULL || pFile_config == NULL ||
       (flags &
        (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
         TINFL_FLAG_COMPUTE_ADLER32)) != 0U) {
        return 0;
    }

    if(decomp_external) {
        memset(decomp, 0, sizeof(*decomp));
    } else if(stack_space >= (stack_budget + decomp_size + stack_decomp_margin)) {
        decomp = TINFL_RUNTIME_ALLOCA(sizeof(tinfl_decompressor));
        if(decomp != NULL) {
            memset(decomp, 0, sizeof(*decomp));
            decomp_on_stack = true;
        }
    } else if(memmgr_heap_get_max_free_block() >= decomp_size) {
        decomp = malloc(decomp_size);
        if(decomp != NULL) {
            memset(decomp, 0, decomp_size);
        }
    }

    if(decomp == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "file_decomp_alloc");
        return 0;
    }
    tinfl_paged_trace(
        &runtime,
        decomp_external ? "file_decomp_external" :
                          (decomp_on_stack ? "file_decomp_stack" : "file_decomp_heap"));

    tinfl_paged_trace(&runtime, "file_dict_attempt");
    if(stack_space >= (stack_budget + decomp_size + dict_size + stack_dict_margin)) {
        dict = TINFL_RUNTIME_ALLOCA(sizeof(tinfl_file_dict));
        if(dict != NULL) {
            memset(dict, 0, sizeof(*dict));
            dict_on_stack = true;
        }
    } else if(memmgr_heap_get_max_free_block() >= sizeof(tinfl_file_dict)) {
        dict = malloc(sizeof(tinfl_file_dict));
        if(dict != NULL) {
            memset(dict, 0, sizeof(*dict));
        }
    }
    if(dict == NULL) {
        tinfl_paged_trace(&runtime, "file_dict_alloc_fail");
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "window_dict_alloc");
        memzero(decomp, sizeof(*decomp));
        if(!decomp_on_stack && !decomp_external) {
            free(decomp);
        }
        return 0;
    }
    tinfl_paged_trace(&runtime, dict_on_stack ? "file_dict_stack" : "file_dict_heap");
    tinfl_paged_trace(&runtime, "file_dict_config_begin");
    if(!tinfl_file_dict_alloc(dict, pFile_config, pTelemetry)) {
        tinfl_paged_trace(&runtime, "file_alloc_failed");
        tinfl_file_dict_free(dict);
        if(!dict_on_stack) {
            free(dict);
        }
        memzero(decomp, sizeof(*decomp));
        if(!decomp_on_stack && !decomp_external) {
            free(decomp);
        }
        return 0;
    }

    tinfl_paged_trace(&runtime, "file_alloc_ok");
    dict_view.ops = &tinfl_file_dict_ops;
    dict_view.impl = dict;
    dict->runtime = &runtime;
    tinfl_init(decomp);
    tinfl_paged_trace(&runtime, "file_begin");

    for(;;) {
        size_t in_buf_size = *pIn_buf_size - in_buf_ofs;
        size_t dst_buf_size = TINFL_LZ_DICT_SIZE - dict_ofs;
        if(pTelemetry != NULL) {
            pTelemetry->loop_count++;
            pTelemetry->last_dict_offset = dict_ofs;
        }
        if(pTelemetry != NULL && pTelemetry->loop_count == 1U) {
            tinfl_paged_trace(&runtime, "file_first_call");
        }
        if(pTelemetry != NULL && pTelemetry->loop_count <= 2U) {
            tinfl_paged_trace(&runtime, "file_call_begin");
        }

        const tinfl_status status = tinfl_decompress_paged(
            decomp,
            (const mz_uint8*)pIn_buf + in_buf_ofs,
            &in_buf_size,
            &dict_view,
            dict_ofs,
            &dst_buf_size,
            (mz_uint32)(flags & ~(TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)),
            &runtime);

        if(pTelemetry != NULL) {
            pTelemetry->last_status = status;
            pTelemetry->last_input_advance = in_buf_size;
            pTelemetry->last_output_advance = dst_buf_size;
        }
        if(pTelemetry != NULL && pTelemetry->loop_count <= 2U) {
            tinfl_paged_trace(&runtime, "file_call_return");
        }

        in_buf_ofs += in_buf_size;
        if(pTelemetry != NULL) {
            pTelemetry->input_offset = in_buf_ofs;
        }

        if(runtime.telemetry != NULL && runtime.telemetry->timed_out) {
            tinfl_paged_trace(&runtime, "timeout");
            break;
        }

        if(dict->storage_failed) {
            tinfl_paged_trace(&runtime, "file_storage_failed");
            break;
        }

        if(in_buf_size == 0U && dst_buf_size == 0U) {
            if(pTelemetry != NULL) {
                pTelemetry->no_progress_count++;
            }
            tinfl_paged_trace(&runtime, "no_progress");
            break;
        }

        if(dst_buf_size > 0U &&
           !dict_view.ops->flush(
               dict_view.impl, dict_ofs, dst_buf_size, pPut_buf_func, pPut_buf_user)) {
            tinfl_paged_trace(&runtime, "flush_rejected");
            break;
        }
        if(dst_buf_size > 0U && pTelemetry != NULL) {
            pTelemetry->flush_count++;
            tinfl_paged_trace(&runtime, "flush");
        }

        if(status != TINFL_STATUS_HAS_MORE_OUTPUT) {
            result = (status == TINFL_STATUS_DONE) && !dict->storage_failed;
            tinfl_paged_trace(&runtime, result ? "done" : "inflate_failed");
            break;
        }

        dict_ofs = (dict_ofs + dst_buf_size) & (TINFL_LZ_DICT_SIZE - 1U);
    }

    *pIn_buf_size = in_buf_ofs;
    tinfl_file_dict_free(dict);
    if(!dict_on_stack) {
        free(dict);
    }
    memzero(decomp, sizeof(*decomp));
    if(!decomp_on_stack && !decomp_external) {
        free(decomp);
    }
    return result;
}

int tinfl_file_paged_probe(
    const tinfl_paged_file_config* pFile_config,
    tinfl_paged_telemetry* pTelemetry) {
    const uint32_t stack_space = furi_thread_get_stack_space(furi_thread_get_current_id());
    const uint32_t stack_budget = 1536U;
    tinfl_decompressor* decomp = NULL;
    const size_t decomp_size = sizeof(tinfl_decompressor);
    const uint32_t stack_decomp_margin = 1024U;
    bool decomp_on_stack = false;
    uint8_t* input_buf = NULL;
    tinfl_file_dict* dict = NULL;
    const size_t dict_size = sizeof(tinfl_file_dict);
    const uint32_t stack_dict_margin = 2048U;
    bool dict_on_stack = false;
    int result = 0;

    tinfl_paged_telemetry_reset(pTelemetry);
    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_enter");

    if(pFile_config == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "window_config");
        return 0;
    }
    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_stack_ok");

    if(stack_space >= (stack_budget + decomp_size + stack_decomp_margin)) {
        decomp = TINFL_RUNTIME_ALLOCA(sizeof(tinfl_decompressor));
        if(decomp != NULL) {
            memset(decomp, 0, sizeof(*decomp));
            decomp_on_stack = true;
        }
    } else if(memmgr_heap_get_max_free_block() >= sizeof(tinfl_decompressor)) {
        decomp = malloc(sizeof(tinfl_decompressor));
        if(decomp != NULL) {
            memset(decomp, 0, sizeof(*decomp));
        }
    }
    if(decomp == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "file_decomp_alloc");
        goto cleanup;
    }
    tinfl_paged_trace_telemetry(
        pTelemetry,
        decomp_on_stack ? "file_probe_decomp_stack" : "file_probe_decomp_heap");
    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_decomp_ok");

    input_buf = malloc(TINFL_PAGED_INPUT_CHUNK_BYTES);
    if(input_buf == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "window_input_alloc");
        goto cleanup;
    }
    memset(input_buf, 0, TINFL_PAGED_INPUT_CHUNK_BYTES);
    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_input_ok");

    if(stack_space >= (stack_budget + decomp_size + dict_size + stack_dict_margin)) {
        dict = TINFL_RUNTIME_ALLOCA(sizeof(tinfl_file_dict));
        if(dict != NULL) {
            memset(dict, 0, sizeof(*dict));
            dict_on_stack = true;
        }
    } else if(memmgr_heap_get_max_free_block() >= sizeof(tinfl_file_dict)) {
        dict = malloc(sizeof(tinfl_file_dict));
        if(dict != NULL) {
            memset(dict, 0, sizeof(*dict));
        }
    }
    if(dict == NULL) {
        tinfl_file_dict_note_budget_issue(NULL, pTelemetry, "window_dict_alloc");
        goto cleanup;
    }
    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_dict_ok");

    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_window_begin");
    if(!tinfl_file_dict_alloc(dict, pFile_config, pTelemetry)) {
        tinfl_paged_trace_telemetry(pTelemetry, "file_probe_window_fail");
        goto cleanup;
    }
    tinfl_paged_trace_telemetry(pTelemetry, "file_probe_window_ok");

    result = 1;

cleanup:
    if(dict != NULL) {
        tinfl_file_dict_free(dict);
        if(!dict_on_stack) {
            free(dict);
        }
    }
    if(input_buf != NULL) {
        memzero(input_buf, TINFL_PAGED_INPUT_CHUNK_BYTES);
        free(input_buf);
    }
    if(decomp != NULL) {
        memzero(decomp, sizeof(*decomp));
        if(!decomp_on_stack) {
            free(decomp);
        }
    }

    return result;
}

int tinfl_decompress_mem_to_callback_paged(
    const void* pIn_buf,
    size_t* pIn_buf_size,
    tinfl_put_buf_func_ptr pPut_buf_func,
    void* pPut_buf_user,
    int flags) {
    return tinfl_decompress_mem_to_callback_paged_ex(
        pIn_buf, pIn_buf_size, pPut_buf_func, pPut_buf_user, flags, NULL);
}

#ifdef __cplusplus
}
#endif

#endif
