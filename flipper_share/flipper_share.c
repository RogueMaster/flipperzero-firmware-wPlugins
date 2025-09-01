#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#include <toolbox/md5_calc.h>

#include <applications/services/notification/notification.h>    // NotificationApp
#include <notification/notification_messages.h>

#define TAG "FlipperShare"

#include "flipper_share.h"
#include "subghz_share.h"

#define FS_RECEIVER_DIRECTORY "inbox"   //TODO: move to .h?

// ===== Constants of protocol / timings =====
enum {
    FS_VERSION             = 1,
    FS_ANNOUNCE_INTERVAL_MS= 3000,   // announce interval
    FS_RX_TIMEOUT_MS       = 500,    // request timeout
    FS_TX_TIMEOUT_MS       = 100,    // beetween data, min 70 ms?
    FS_IDLE_TICK_MS        = 50      // interval for calling fs_idle()  // TODO, move to .h?
};

// ===== Global state =====

fs_ctx_t g; //static, extern in .h

void fs_notify_success(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    // notification_message(notification, &sequence_single_vibro);
    notification_message(notification, &sequence_success);
    furi_record_close(RECORD_NOTIFICATION);
}

void fs_notify_error(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_error);
    furi_record_close(RECORD_NOTIFICATION);
}

void fs_notify_led_red(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_red_10);
    furi_record_close(RECORD_NOTIFICATION);
}

void fs_notify_led_green(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_green_10);
    furi_record_close(RECORD_NOTIFICATION);
}

void fs_notify_led_blue(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_blue_10);
    furi_record_close(RECORD_NOTIFICATION);
}

// ===== Bit map for received blocks (1 bit per block) =====================

// Map
typedef struct {
    uint8_t  *bits;     // array of bytes, 1 bit = 1 block
    uint32_t  count;    // number of blocks (valid bits)
    uint32_t  nbytes;   // length of bits array in bytes
} fs_blockmap_t;

static fs_blockmap_t g_map = {0};

static inline uint32_t _fs_div8(uint32_t x) { return x >> 3; }
static inline uint32_t _fs_mod8(uint32_t x) { return x & 7u; }

bool fs_map_init(uint32_t block_count) {    // default all blocks = 0 (means not received)
    if (g_map.bits) {
        free(g_map.bits);
        g_map.bits = NULL;
    }
    g_map.count  = 0;
    g_map.nbytes = 0;

    if (block_count == 0) return true; // empty map is allowed

    uint32_t nbytes = (block_count + 7u) / 8u;      // calculate buffer size and check for overflow

    // simple overflow check for size_t in malloc
    if (nbytes == 0 || nbytes > (uint32_t)SIZE_MAX) return false;

    uint8_t *buf = (uint8_t*)malloc(nbytes);
    if (!buf) return false;

    memset(buf, 0x00, nbytes);

    // Make tail bits (beyond count) = 1, to avoid false positives in zero search
    uint32_t rem = block_count & 7u; // count % 8
    if (rem != 0) {
        uint8_t tail_mask = (uint8_t)~((1u << rem) - 1u); // bits beyond count
        buf[nbytes - 1] |= tail_mask;
    }

    g_map.bits   = buf;
    g_map.count  = block_count;
    g_map.nbytes = nbytes;
    return true;
}

void fs_map_deinit(void) {
    if (g_map.bits) {
        free(g_map.bits);
        g_map.bits = NULL;
    }
    g_map.count  = 0;
    g_map.nbytes = 0;
}

bool fs_map_set(uint32_t block_number, uint8_t value) {     // Set numbered block to 0/1, return false if out of range
    if (!g_map.bits || block_number >= g_map.count) return false;
    uint32_t i   = _fs_div8(block_number);
    uint8_t  m   = (uint8_t)(1u << _fs_mod8(block_number));
    if (value)   g_map.bits[i] |=  m;
    else         g_map.bits[i] &= (uint8_t)~m;
    return true;
}

int fs_map_get(uint32_t block_number) {     // Get numbered block value 0/1, -1 if out of range
    if (!g_map.bits || block_number >= g_map.count) return -1;
    uint32_t i = _fs_div8(block_number);
    uint8_t  m = (uint8_t)(1u << _fs_mod8(block_number));
    return (g_map.bits[i] & m) ? 1 : 0;
}

// Find first num with value 0/1, starting from offset_from (inclusive). Returns UINT32_MAX if not found
uint32_t fs_map_search(uint8_t bitval, uint32_t offset_from) {  
    if (!g_map.bits) return UINT32_MAX;
    if (offset_from >= g_map.count) return UINT32_MAX;

    uint32_t byte_idx = _fs_div8(offset_from);
    uint32_t bit_off  = _fs_mod8(offset_from);

    // first (partial) iteration — mask bits before offset_from
    {
        uint8_t byte = g_map.bits[byte_idx];
        if (bitval == 0) byte = (uint8_t)~byte; // searching for zeros => invert

        // mask: keep bits [bit_off..7]
        uint8_t mask = (uint8_t)(0xFFu << bit_off);
        uint8_t cand = (uint8_t)(byte & mask);
        if (cand) {
            // find first set bit
            for (uint32_t b = bit_off; b < 8; ++b) {
                if (cand & (1u << b)) {
                    uint32_t idx = (byte_idx << 3) + b;
                    if (idx < g_map.count) return idx;
                    else return UINT32_MAX;
                }
            }
        }
        byte_idx++;
    }

    // full bytes
    for (; byte_idx < g_map.nbytes; ++byte_idx) {
        uint8_t byte = g_map.bits[byte_idx];
        if (bitval == 0) byte = (uint8_t)~byte;

        if (byte) { // if it is last byte, protect against overflow count
            bool last = (byte_idx == g_map.nbytes - 1);
            for (uint32_t b = 0; b < 8; ++b) {
                if (byte & (1u << b)) {
                    uint32_t idx = (byte_idx << 3) + b;
                    if (!last || idx < g_map.count) return idx;
                    else return UINT32_MAX;
                }
            }
        }
    }

    return UINT32_MAX;
}

bool fs_map_all_set(void) {     // Quick check if all blocks received, true if all bits is 1
    if (!g_map.bits) return false;
    // all intermediate bytes must be 0xFF
    for (uint32_t i = 0; i + 1 < g_map.nbytes; ++i) {
        if (g_map.bits[i] != 0xFFu) return false;
    }
    // last byte must also be 0xFF, since we set "tail" bits to 1 during init
    return g_map.bits[g_map.nbytes - 1] == 0xFFu;
}

static uint8_t fs_crc8(const uint8_t* data, size_t len) {   // CRC-8-ATM, polynom 0x07
    uint8_t crc = 0x00; // init
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

typedef enum {
    FS_PARTS_MODE_NONE = 0,
    FS_PARTS_MODE_BLOCKS_PER_PART, // B >= N
    FS_PARTS_MODE_PARTS_PER_BLOCK  // B <  N
} fs_parts_mode_t;

static struct {
    uint32_t         B;     // block_count
    uint32_t         N;     // parts count (= FS_PARTS_COUNT)
    fs_parts_mode_t  mode;
    // Bitmap of parts received, 1 bit = part full
    uint8_t          bits[FS_PARTS_BYTES];

    // If B>=N, then each part has at least 1 block, so missing[] >= 1 always
    uint32_t         seg_start[FS_PARTS_COUNT];
    uint32_t         seg_end  [FS_PARTS_COUNT];
    uint32_t         missing  [FS_PARTS_COUNT]; // >=1, monotonically decrease to 0
} fs_parts;

static inline void _parts_bit_set(uint32_t s) {
    fs_parts.bits[s >> 3] |= (uint8_t)(1u << (s & 7u));
}
static inline uint8_t _parts_bit_get(uint32_t s) {
    return (fs_parts.bits[s >> 3] >> (s & 7u)) & 1u;
}

static inline uint32_t _scale_floor_u32(uint32_t x, uint32_t mul, uint32_t den) {
    return (uint32_t)(((uint64_t)x * (uint64_t)mul) / (uint64_t)den);
}

// Init pre-calculated if B>=N
static void _init_blocks_per_part(void) {
    const uint32_t B = fs_parts.B;
    const uint32_t N = fs_parts.N;

    for (uint32_t s = 0; s < N; ++s) {
        uint32_t start = _scale_floor_u32(s,     B, N);
        uint32_t end   = _scale_floor_u32(s + 1, B, N) - 1u;
        fs_parts.seg_start[s] = start;
        fs_parts.seg_end[s]   = end;
        fs_parts.missing[s]   = (end >= start) ? (end - start + 1u) : 0u; // if B>=N, then always >=1
    }
}

bool fs_parts_init(uint32_t block_count) {
    memset(&fs_parts, 0, sizeof(g));
    fs_parts.N = FS_PARTS_COUNT;
    fs_parts.B = block_count;

    if (fs_parts.N == 0) {
        fs_parts.mode = FS_PARTS_MODE_NONE;
        return false;
    }

    memset(fs_parts.bits, 0, sizeof(fs_parts.bits));

    if (fs_parts.B == 0) {
        fs_parts.mode = FS_PARTS_MODE_NONE;
        return true;
    }

    if (fs_parts.B >= fs_parts.N) {
        fs_parts.mode = FS_PARTS_MODE_BLOCKS_PER_PART;
        _init_blocks_per_part();
    } else {
        fs_parts.mode = FS_PARTS_MODE_PARTS_PER_BLOCK;  // no need to pre-calculate anything
    }
    return true;
}

void fs_parts_reset(void) {
    memset(&fs_parts, 0, sizeof(g));
}

void fs_parts_on_block_set(uint32_t i) {
    if (fs_parts.B == 0 || fs_parts.N == 0 || fs_parts.mode == FS_PARTS_MODE_NONE) return;
    if (i >= fs_parts.B) return; // out of range

    if (fs_parts.mode == FS_PARTS_MODE_BLOCKS_PER_PART) {
        // Find part that block i belongs to
        uint32_t s = _scale_floor_u32(i, fs_parts.N, fs_parts.B);
        if (s >= fs_parts.N) return; // bounds check

        // If already set, do nothing
        if (_parts_bit_get(s)) return;

        // Decrement "how many still not received"
        if (fs_parts.missing[s] > 0) {
            fs_parts.missing[s]--;
            if (fs_parts.missing[s] == 0) {
                _parts_bit_set(s);
            }
        }
    } else {
        // FS_PARTS_MODE_PARTS_PER_BLOCK: block covers range of parts [sf .. sl]
        // sf = floor(i*N/B), sl = floor((i+1)*N/B) - 1
        uint32_t sf = _scale_floor_u32(i,     fs_parts.N, fs_parts.B);
        uint32_t sl = _scale_floor_u32(i + 1, fs_parts.N, fs_parts.B);
        if (sl > 0) sl -= 1u;

        if (sf >= fs_parts.N) return;
        if (sl >= fs_parts.N) sl = fs_parts.N - 1u;
        if (sf >  sl)  return;

        // Set bits. This is idempotent, each bit is set at most once per session.
        for (uint32_t s = sf; s <= sl; ++s) {
            _parts_bit_set(s);
        }
    }
}

int fs_parts_get(uint32_t part_index) {
    if (part_index >= fs_parts.N || fs_parts.N == 0) return -1;
    return _parts_bit_get(part_index) ? 1 : 0;
}

void fs_parts_bitmap_copy(uint8_t* dst) {
    if (!dst) return;
    memcpy(dst, fs_parts.bits, FS_PARTS_BYTES);
}

uint32_t fs_parts_count(void) {
    return fs_parts.N;
}

uint32_t fs_parts_block_count(void) {
    return fs_parts.B;
}

bool fs_parts_is_ready(void) {
    // Assume "ready" if N>0 and (B==0 or some mode selected)
    if (fs_parts.N == 0) return false;
    if (fs_parts.B == 0) return true;
    return fs_parts.mode == FS_PARTS_MODE_BLOCKS_PER_PART || fs_parts.mode == FS_PARTS_MODE_PARTS_PER_BLOCK;
}


// ===== Helpers =====

const char* fs_basename(const char* path) {
    if (!path || !*path) return "";  // empty

    const char* end = path + strlen(path); // end points to '\0'

    while (end > path && end[-1] == '/') end--;  // remove trailing '/'

    if (end == path) return "/";   // string consisted only of '/' (e.g. "/","///")

    const char* p = end;
    while (p > path && p[-1] != '/') p--;   // find last '/'

    return p; // [p, end) — basename
}

// Zero‑pad payload to 56 bytes when packing REQUEST etc.
static void fs_zero_pad(uint8_t* payload, size_t used) {
    if (used < FS_PAYLOAD_LENGTH) {
        memset(payload + used, 0, FS_PAYLOAD_LENGTH - used);
    }
}

void fs_hexdump(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < len; ++i) {
        if ((i % 16) == 0) printf("%04zu: ", i);
        printf("%02X ", p[i]);
        if ((i % 16) == 15 || i + 1 == len) printf("\n");
    }
}

// ===== Serialization/Deserialization of Packets =====

static void fs_packet_pack(FS_packet_t* out, uint8_t version, uint8_t tx_id, uint8_t pkt_type, const void* payload_src) {
    out->version     = version;
    out->tx_id       = tx_id;
    out->packet_type = pkt_type;
    memcpy(out->payload, payload_src, FS_PAYLOAD_LENGTH);
    out->crc = fs_crc8((const uint8_t*)out, FS_PACKET_LENGTH - 1); // all except CRC
}

static bool fs_packet_check_and_parse(const uint8_t* buf, size_t len, FS_packet_t* out) {
    if (len != FS_PACKET_LENGTH) return false;
    memcpy(out, buf, FS_PACKET_LENGTH);
    uint8_t calc = fs_crc8(buf, FS_PACKET_LENGTH - 1);
    if (calc != out->crc) return false;
    if (out->version != FS_VERSION) return false;
    return true;
}

// ===== Payloads: pack/unpack =====

// ANNOUNCE
static void fs_pl_announce_pack(uint8_t payload_out[FS_PAYLOAD_LENGTH],
                                const FS_pl_announce_t* pl_src) {
    memcpy(payload_out, pl_src->file_name, FS_FILENAME_LENGTH);
    memcpy(payload_out + FS_FILENAME_LENGTH, &pl_src->file_size, sizeof(uint32_t)); // LE
    memcpy(payload_out + FS_FILENAME_LENGTH + 4, pl_src->hash_md5, 16);
}

static void fs_pl_announce_unpack(const uint8_t payload[FS_PAYLOAD_LENGTH],
                                  FS_pl_announce_t* pl_out) {
    memcpy(pl_out->file_name, payload, FS_FILENAME_LENGTH);
    memcpy(&pl_out->file_size, payload + FS_FILENAME_LENGTH, sizeof(uint32_t));
    memcpy(pl_out->hash_md5, payload + FS_FILENAME_LENGTH + 4, 16);
}

// REQUEST (8 bytes + zero‑pad)
static void fs_pl_request_pack(uint8_t payload_out[FS_PAYLOAD_LENGTH],
                               const FS_pl_request_t* pl_src) {
    memcpy(payload_out + 0, &pl_src->range_start, sizeof(uint32_t));
    memcpy(payload_out + 4, &pl_src->range_end,   sizeof(uint32_t));
    fs_zero_pad(payload_out, 8);
}

static void fs_pl_request_unpack(const uint8_t payload[FS_PAYLOAD_LENGTH],
                                 FS_pl_request_t* pl_out) {
    memcpy(&pl_out->range_start, payload + 0, sizeof(uint32_t));
    memcpy(&pl_out->range_end,   payload + 4, sizeof(uint32_t));
}

// DATA (exactly FS_DATA_LENGTH)
static void fs_pl_data_pack(uint8_t payload_out[FS_PAYLOAD_LENGTH],
                            const FS_pl_data_t* pl_src) {
    memcpy(payload_out + 0, &pl_src->block_number, sizeof(uint32_t));
    memcpy(payload_out + 4, pl_src->data, FS_DATA_LENGTH);
}

static void fs_pl_data_unpack(const uint8_t payload[FS_PAYLOAD_LENGTH],
                              FS_pl_data_t* pl_out) {
    memcpy(&pl_out->block_number, payload + 0, sizeof(uint32_t));
    memcpy(pl_out->data,          payload + 4, FS_DATA_LENGTH);
}

// ===== API Implementation =====

void subghz_send_bytes(const uint8_t* buf, size_t len) {
    // FURI_LOG_I(TAG, "subghz_send_bytes: %zu bytes", len);
    uint8_t res = subghz_share_send((uint8_t*)buf, len);
    if(res != 0) {
        if(res == 3) {
            FURI_LOG_W(TAG, "subghz_send_bytes: Retry in %dms:", FS_SUBGHZ_RETRY_DELAY_MS);
            furi_delay_ms(FS_SUBGHZ_RETRY_DELAY_MS);
            res = subghz_share_send((uint8_t*)buf, len);
            if (res != 0) {
                FURI_LOG_E(TAG, "subghz_send_bytes: Retry failed");
            }
        }
        FURI_LOG_E(TAG, "subghz_send_bytes: Retry: Failed to send data");
    }
}

void fs_ensure_inbox_dir(void) {
    g.storage = furi_record_open(RECORD_STORAGE);
    if(!g.storage) return;

    bool ok = storage_simply_mkdir(g.storage, EXT_PATH(FS_RECEIVER_DIRECTORY));
    if(!ok) {
        FURI_LOG_E(TAG, "fs_ensure_inbox_dir: Failed to create directory '%s'", EXT_PATH(FS_RECEIVER_DIRECTORY));
    }
    // TODO: check return value?

    furi_record_close(RECORD_STORAGE);
}

uint8_t fs_file_create_truncate() {
    FURI_LOG_I(TAG, "fs_file_create_truncate: Creating and truncating file '%s'", g.r_file_path);
    if (g.mode != FS_MODE_RECEIVER) {
        FURI_LOG_E(TAG, "fs_file_create_truncate: Not available in SENDER mode");
        return 1;
    }

    g.storage = furi_record_open(RECORD_STORAGE);
    g.file = storage_file_alloc(g.storage);
    if(!g.file) {
        FURI_LOG_E(TAG, "fs_file_create_truncate: Failed to allocate file handle");
        return 2;
    }
    if(!storage_file_open(g.file, g.r_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "fs_file_create_truncate: Failed to open file '%s'", g.r_file_path);
        storage_file_close(g.file);
        storage_file_free(g.file);
        furi_record_close(RECORD_STORAGE);
        return 3;
    }

    storage_file_close(g.file);
    storage_file_free(g.file);
    furi_record_close(RECORD_STORAGE);

    return 0;
}

void my_write_block(uint32_t block_number, const uint8_t in52[FS_DATA_LENGTH], uint32_t valid_len) {
    uint32_t offset = block_number * FS_DATA_LENGTH;
    // FURI_LOG_I(TAG, "my_write_block: block %lu, valid_len %lu", block_number, valid_len);
    
    g.storage = furi_record_open(RECORD_STORAGE);
    g.file = storage_file_alloc(g.storage);
    if(!g.file) {
        FURI_LOG_E(TAG, "my_write_block: Failed to allocate file handle");
        storage_file_close(g.file);
        storage_file_free(g.file);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    if(!storage_file_open(g.file, g.r_file_path, FSAM_WRITE, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "my_write_block: Failed to open file '%s'", g.r_file_path);
        storage_file_close(g.file);
        storage_file_free(g.file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if(!storage_file_seek(g.file, offset, true)) {
        FURI_LOG_E(TAG, "my_write_block: Failed to seek to offset %lu", offset);
        storage_file_close(g.file);
        storage_file_free(g.file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint32_t bytes_written = storage_file_write(g.file, in52, valid_len);
    if (bytes_written < valid_len) {
        FURI_LOG_W(TAG, "my_write_block: Write error, only %lu bytes written", bytes_written);
    }

    storage_file_close(g.file);
    storage_file_free(g.file);
    furi_record_close(RECORD_STORAGE);
    return;
}

uint32_t my_read_block(uint32_t block_number, uint8_t out52[FS_DATA_LENGTH]) {
    // FURI_LOG_I(TAG, "my_read_block: block %lu", block_number);

    g.storage = furi_record_open(RECORD_STORAGE);
    g.file = storage_file_alloc(g.storage);
    if(!g.file) {
        FURI_LOG_E(TAG, "my_read_block: Failed to allocate file handle");
        return 1;
    }
    if(!storage_file_open(g.file, g.s_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "my_read_block: Failed to open file '%s'", g.s_file_path);
        storage_file_close(g.file);
        storage_file_free(g.file);
        furi_record_close(RECORD_STORAGE);
        return 2;
    }

    memset(out52, 0, FS_DATA_LENGTH);

    storage_file_seek(g.file, block_number * FS_DATA_LENGTH, true);

    uint32_t bytes = storage_file_read(g.file, out52, FS_DATA_LENGTH);
    if (bytes < FS_DATA_LENGTH) {
        FURI_LOG_W(TAG, "my_read_block: Read error, last block? %lu bytes read", bytes);
    }

    storage_file_close(g.file);
    storage_file_free(g.file);
    furi_record_close(RECORD_STORAGE);

    return bytes; // number of valid bytes read, can be < FS_DATA_LENGTH for last block
}

bool fs_init_from_external_transmit(const char* file_path) {
    // FURI_LOG_I(TAG, "fs_init_from_external_transmit: file_path='%s'", file_path);

    fs_init_params_t ps;
    memset(&ps, 0, sizeof(ps));
    ps.mode = FS_MODE_SENDER;
    ps.tx_id = (uint8_t)(furi_get_tick() & 0xFFu);
    ps.send_bytes = subghz_send_bytes;
    ps.now_ms = furi_get_tick;

    if(file_path) {
        strncpy(ps.s_file_path, file_path, sizeof(ps.s_file_path) - 1);
        ps.s_file_path[sizeof(ps.s_file_path) - 1] = '\0';
    } else {
        ps.s_file_path[0] = '\0';
    }
    ps.s_read_block = my_read_block;
    return fs_init(&ps);
}

bool fs_init_from_external_receive() {
    fs_init_params_t pr = {
      .mode = FS_MODE_RECEIVER,
      .tx_id = 0x00,
      .send_bytes = subghz_send_bytes,
      .now_ms = furi_get_tick,
      .r_write_block = my_write_block
    };
    return fs_init(&pr);
}

bool fs_init(const fs_init_params_t* p) {
    memset(&g, 0, sizeof(g));

    if (!p || !p->send_bytes || !p->now_ms) {
        FURI_LOG_E(TAG, "fs_init: Invalid parameters");
        return false;
    }
    if (p->mode != FS_MODE_SENDER && p->mode != FS_MODE_RECEIVER) {
        FURI_LOG_E(TAG, "fs_init: Invalid mode %d", p->mode);
        return false;
    }

    g.mode = p->mode;

    g.state = (p->mode == FS_MODE_SENDER) ? FS_ST_ANNOUNCING : FS_ST_RECEIVING;
    g.cb_send_bytes = p->send_bytes;
    g.cb_now_ms     = p->now_ms;
    g.last_tick_ms = g.last_announce_ms = g.last_rx_ms = g.cb_now_ms();
    
    if (g.mode == FS_MODE_SENDER) {
        g.tx_id = p->tx_id;
        FURI_LOG_I(TAG, "fs_init: SENDER mode, file_path='%s'", p->s_file_path);

        // Process target file metadata:

        // copy file_path from p->s_file_path (s_file_path is an array -> check contents)
        if (p->s_file_path[0] == '\0') {
            FURI_LOG_E(TAG, "fs_init: Invalid file path");
            return false;
        }

        strncpy((char*)g.s_file_path, (const char*)p->s_file_path, sizeof(g.s_file_path) - 1);
        g.s_file_path[sizeof(g.s_file_path) - 1] = '\0'; // Ensure null-termination

        // get basename from file_path to g.s_file_name
        const char* basename = fs_basename((const char*)g.s_file_path);
        if (strlen(basename) >= FS_FILENAME_LENGTH) {
            FURI_LOG_E(TAG, "fs_init: File name too long: %s", basename);
            return false;
        }
        g.s_file_name[0] = '\0'; // Zero-initialize
        strncpy((char*)g.s_file_name, basename, FS_FILENAME_LENGTH - 1);
        g.s_file_name[FS_FILENAME_LENGTH - 1] = '\0'; // Ensure null-termination

        g.s_file_size = 0;
        g.storage = furi_record_open(RECORD_STORAGE);
        g.file = storage_file_alloc(g.storage);
        if (g.file) {
            if (storage_file_open(g.file, g.s_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
                g.s_file_size = storage_file_size(g.file);
            } else {
                FURI_LOG_E(TAG, "fs_init: Failed to open file '%s'", g.s_file_path);
                g.s_file_size = 0; // Reset file size on failure
                storage_file_free(g.file);
                furi_record_close(RECORD_STORAGE);
                return false;
            }
        } else {
            FURI_LOG_E(TAG, "fs_init: Invalid file name or storage");
            return false;
        }
        if (g.storage) {
            storage_file_close(g.file);
            // storage_file_free(g.file);
            // furi_record_close(RECORD_STORAGE);
            // g.storage = NULL; // storage is not needed after file open
        }
        
        g.s_md5[0] = 0; // Zero-initialize MD5
        FS_Error err = 0;
        md5_calc_file(g.file, g.s_file_path, g.s_md5, &err);

        if (g.storage) {
            storage_file_free(g.file);
            furi_record_close(RECORD_STORAGE);
            g.storage = NULL;
        }


        if (!p->s_read_block) return false;
        g.cb_read_block = p->s_read_block;

        g.s_is_blocks_requested = 0;
        g.s_block_needed_first = 0;
        g.s_block_needed_last = 0;

        FURI_LOG_I(TAG, "fs_init: SENDER, file_path='%s', file_name='%s', file_size=%lu, mode=%d, tx_id=%d",
            g.s_file_path, g.s_file_name, g.s_file_size, g.mode, g.tx_id);
        FURI_LOG_I(TAG, "fs_init: md5=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            g.s_md5[0], g.s_md5[1], g.s_md5[2], g.s_md5[3],
            g.s_md5[4], g.s_md5[5], g.s_md5[6], g.s_md5[7],
            g.s_md5[8], g.s_md5[9], g.s_md5[10], g.s_md5[11],
            g.s_md5[12], g.s_md5[13], g.s_md5[14], g.s_md5[15]);
        
    } else { // RECEIVER:
        if (!p->r_write_block) return false;
        g.cb_write_block = p->r_write_block;
        g.r_locked = false;
        g.r_is_finished = false;
        g.r_is_success = false;
        g.tx_id = 0; // will be set from ANNOUNCE
        FURI_LOG_I(TAG, "fs_init: RECEIVER");
    }
    return true;
}

void fs_deinit(void) {
    FURI_LOG_I(TAG, "fs_deinit");

    // close file handle if opened
    if (g.storage) {
        storage_file_close(g.file);
        storage_file_free(g.file);
        furi_record_close(RECORD_STORAGE);
        g.storage = NULL;
        g.file = NULL;
    }

    // free block map
    fs_map_deinit();

    // clear callbacks and other pointers to avoid use-after-free
    g.cb_send_bytes = NULL;
    g.cb_now_ms     = NULL;
    g.cb_read_block = NULL;
    g.cb_write_block = NULL;

    // zero whole context after resources freed
    memset(&g, 0, sizeof(g));

    FURI_LOG_I(TAG, "fs_deinit: done");
}

// High-level sender helper functions

void fs_send_announce(void) {
    if (g.mode != FS_MODE_SENDER || !g.cb_send_bytes) return;

    FURI_LOG_I(TAG, "fs_send_announce: file_name='%s', file_size=%lu", g.s_file_name, g.s_file_size);

    fs_notify_led_blue();

    FS_pl_announce_t pl = {0};
    memcpy(pl.file_name, g.s_file_name, FS_FILENAME_LENGTH);
    pl.file_size = g.s_file_size;
    memcpy(pl.hash_md5, g.s_md5, 16);

    uint8_t payload[FS_PAYLOAD_LENGTH];
    fs_pl_announce_pack(payload, &pl);

    FS_packet_t pkt;
    fs_packet_pack(&pkt, FS_VERSION, g.tx_id, FS_PKT_ANNOUNCE, payload);
    g.cb_send_bytes((const uint8_t*)&pkt, sizeof(pkt));
}

void fs_send_request(uint32_t range_start, uint32_t range_end) {
    // FURI_LOG_I(TAG, "fs_send_request: range_start=%lu, range_end=%lu", range_start, range_end);
    
    if (g.mode != FS_MODE_RECEIVER || !g.cb_send_bytes) return;

    // RECOMMENDATION: align range by FS_DATA_LENGTH, except the tail == file_size
    FS_pl_request_t pl = { .range_start = range_start, .range_end = range_end };
    uint8_t payload[FS_PAYLOAD_LENGTH];
    fs_pl_request_pack(payload, &pl);

    FS_packet_t pkt;
    fs_packet_pack(&pkt, FS_VERSION, g.tx_id, FS_PKT_REQUEST, payload);
    g.cb_send_bytes((const uint8_t*)&pkt, sizeof(pkt));
}

void fs_send_data() {
    if (g.mode != FS_MODE_SENDER || !g.cb_send_bytes || !g.cb_read_block) return;

    if (g.s_is_blocks_requested == 0) {
        FURI_LOG_E(TAG, "fs_send_data: No blocks needed, cannot send data");
        return;
    }

    uint32_t block_number = g.s_block_needed_first;
    if (g.s_block_needed_first < g.s_block_needed_last) {
        g.s_block_needed_first++;
    } else {
        g.s_is_blocks_requested = 0; // all requested blocks have been sent
    }

    FURI_LOG_I(TAG, "fs_send_data: block_number=%lu", block_number);

    uint8_t data52[FS_DATA_LENGTH];
    uint32_t valid = g.cb_read_block(block_number, data52); // fill and zero‑pad if valid<FS_DATA_LENGTH

    fs_notify_led_green();

    (void)valid; // receiver computes final length using file_size

    FS_pl_data_t pl = {0};
    pl.block_number = block_number;
    memcpy(pl.data, data52, FS_DATA_LENGTH);

    uint8_t payload[FS_PAYLOAD_LENGTH];
    fs_pl_data_pack(payload, &pl);

    FS_packet_t pkt;
    fs_packet_pack(&pkt, FS_VERSION, g.tx_id, FS_PKT_DATA, payload);
    g.cb_send_bytes((const uint8_t*)&pkt, sizeof(pkt));
}

// ===== Simple timer/behavior logic =====

void fs_idle(void) {
    if (!g.cb_now_ms) return;
    uint32_t now = g.cb_now_ms();

    if (g.mode == FS_MODE_SENDER) {
        if (g.state == FS_ST_ANNOUNCING) {
            if (now - g.last_announce_ms >= FS_ANNOUNCE_INTERVAL_MS) {
                fs_send_announce();
                g.last_announce_ms = now;
            }
        }    
        if (now - g.last_rx_ms > FS_TX_TIMEOUT_MS) {    // TODO: g.last_tick_ms
            if (g.s_is_blocks_requested == 1) {
                fs_send_data();
            }
        }
    }

    if (g.mode == FS_MODE_RECEIVER) {
        if (g.r_is_finished) {
            // FURI_LOG_I(TAG, "fs_idle: Receiver finished, reopen to try again.");
            return;
        }
        if (now - g.last_rx_ms > FS_RX_TIMEOUT_MS) {
            // You may clear the receiver lock or restart announce on the sender
            // if (g.mode == FS_MODE_RECEIVER) {
            //     g.r_locked = false; // release session lock
            // }
            // If tx_id locked and blocks are needed: calculate range and send REQUEST, TODO
            if (g.r_locked && g.r_is_finished == false) {
                if (g.r_blocks_received < g.r_blocks_needed) { // find next needed block window
                    uint32_t nblk_start = fs_map_search(0, 0);
                    uint32_t nblk_end = fs_map_search(1, nblk_start + 1);
                    uint32_t nbyte_start = nblk_start * FS_DATA_LENGTH;
                    if (nblk_end == UINT32_MAX) {
                        nblk_end = g.r_blocks_needed - 1; // last block
                    }
                    uint32_t nbyte_end = (nblk_end + 1) * FS_DATA_LENGTH; // inclusive end
                    
                    // Queueing:
                    // Check how many blocks are still needed
                    // If the more blocks are needed, the later we send a request for them
                    // So that the transmitter processes the request from the one who has already received almost everything earlier
                    uint32_t blocks_left = g.r_blocks_needed - g.r_blocks_received;
                    uint32_t wait_before_request = (blocks_left * 300) / g.r_blocks_needed; // ms

                    FURI_LOG_I(TAG, "fs_idle: sending REQUEST for blocks range (%lu, %lu), bytes (%lu, %lu), delay=%lums",
                        nblk_start, nblk_end, nbyte_start, nbyte_end, wait_before_request);

                    furi_delay_ms(wait_before_request);

                    furi_delay_ms((uint8_t)((furi_get_tick() % 30))); // Random jitter, ms

                    fs_send_request(nbyte_start, nbyte_end);
                }
            }
            g.last_rx_ms = now; // debounce timer
        }
        if (g.r_locked && g.r_blocks_received == g.r_blocks_needed) {
            FURI_LOG_I(TAG, "fs_idle: ALL THE BLOCKS RECEIVED, resetting state?");

            g.r_locked = false; // release session lock
            g.r_locked_tx_id = 0;
            g.r_blocks_received = 0;
            fs_map_deinit(); // reset block map

            // Calculate MD5 and compare with announced
            g.storage = furi_record_open(RECORD_STORAGE);
            g.file = storage_file_alloc(g.storage);
            
            g.r_md5[0] = 0; // Zero-initialize MD5
            FS_Error err = 0;
            md5_calc_file(g.file, g.r_file_path, g.r_md5, &err);

            if (err != 0) {
                FURI_LOG_E(TAG, "fs_idle: md5_calc_file error %d", err);
            } else {
                g.r_is_success = true;
                FURI_LOG_I(TAG, "fs_idle: Received file md5=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                    g.r_md5[0], g.r_md5[1], g.r_md5[2], g.r_md5[3],
                    g.r_md5[4], g.r_md5[5], g.r_md5[6], g.r_md5[7],
                    g.r_md5[8], g.r_md5[9], g.r_md5[10], g.r_md5[11],
                    g.r_md5[12], g.r_md5[13], g.r_md5[14], g.r_md5[15]);
            }

            storage_file_free(g.file);
            furi_record_close(RECORD_STORAGE);
            g.storage = NULL;

            // compare g.r_md5 with g.r_md5 from ANNOUNCE
            if (memcmp(g.r_md5, g.r_md5, 16) == 0) {
                FURI_LOG_I(TAG, "fs_idle: MD5 match, file received successfully");
                g.r_is_success = true;
            } else {
                FURI_LOG_W(TAG, "fs_idle: MD5 mismatch, file may be corrupted");
                g.r_is_success = false;
            }

            g.r_is_finished = true; // reception finished flag

            // g.r_is_success = false; // For testing negative case

            if (g.r_is_success) {
                fs_notify_success();
            } else {
                fs_notify_error();
            }

        }
    }
    g.last_tick_ms = now;
}

// ===== Packet reception and mini state machine =====

static void fs_handle_announce(uint8_t tx_id, const FS_pl_announce_t* ann) {
    g.last_rx_ms = g.cb_now_ms ? g.cb_now_ms() : 0;

    // FURI_LOG_I(TAG, "fs_handle_announce: tx_id=%d, file_name='%s', file_size=%lu",
    //          tx_id, ann->file_name, ann->file_size);

    if (g.mode != FS_MODE_RECEIVER) return;

    if (g.r_is_finished == false && g.r_locked == false) {
        FURI_LOG_I(TAG, "fs_handle_announce: LOCK to tx_id=%d, file_name='%s', file_size=%lu",
                 tx_id, ann->file_name, ann->file_size);
        g.r_locked = true;
        g.r_locked_tx_id = tx_id;
        g.tx_id = tx_id; // respond on the same tx_id
        memcpy(g.r_file_name, ann->file_name, FS_FILENAME_LENGTH);
        g.r_file_size = ann->file_size;
        memcpy(g.r_md5, ann->hash_md5, sizeof(g.r_md5));
        g.r_blocks_needed = (g.r_file_size + FS_DATA_LENGTH - 1) / FS_DATA_LENGTH; // round up

        // build fullpath as /ext/<file_name>
        snprintf((char*)g.r_file_path, sizeof(g.r_file_path), "/ext/%s/%s", FS_RECEIVER_DIRECTORY, g.r_file_name);
        FURI_LOG_I(TAG, "fs_handle_announce: r_file_path='%s'", g.r_file_path);

        fs_ensure_inbox_dir();
        if (fs_file_create_truncate()!= 0) {
            FURI_LOG_E(TAG, "fs_handle_announce: Failed to create/truncate file");
            g.r_locked = false; // release lock if failed to create file
            g.r_locked_tx_id = 0;
            return;
        }

        FURI_LOG_I(TAG, "fs_handle_announce: Init fs_map for %lu blocks...", g.r_blocks_needed);
        if (!fs_map_init(g.r_blocks_needed)) {
            FURI_LOG_E(TAG, "fs_handle_announce: Failed to init block map");
            g.r_locked = false; // release lock if failed to init map
            g.r_locked_tx_id = 0;
            return;
        }
        // Init parts for GUI progress bar
        FURI_LOG_I(TAG, "fs_handle_announce: Init fs_parts for %lu blocks...", g.r_blocks_needed);
        if (!fs_parts_init(g.r_blocks_needed)) {
            FURI_LOG_E(TAG, "fs_handle_announce: Failed to init parts");
            return;
        }
    }

    // If already locked on another sender — ignore
    if (g.r_locked && g.r_locked_tx_id != tx_id) {
        fs_notify_led_red();
        return;
    }

    // You may update metadata / resend REQUEST if needed
}

static void fs_handle_request(uint8_t tx_id, const FS_pl_request_t* rq) {
    g.last_rx_ms = g.cb_now_ms ? g.cb_now_ms() : 0;

    FURI_LOG_I(TAG, "fs_handle_request: tx_id=%d, range_start=%lu, range_end=%lu",
             tx_id, rq->range_start, rq->range_end);

    if (g.mode != FS_MODE_SENDER) return;
    if (tx_id != g.tx_id) {
        fs_notify_led_red();
        FURI_LOG_W(TAG, "fs_handle_request: tx_id=%d != g.tx_id=%d, ignoring", tx_id, g.tx_id);
        return;
    }
    
    if (g.s_is_blocks_requested == 1) {
        FURI_LOG_W(TAG, "fs_handle_request: tx_id=%d, already have blocks requested", tx_id);
        return;
    }
    fs_notify_led_green();

    // Normalize to blocks (uneven tail == file_size allowed)
    uint32_t start = rq->range_start;
    uint32_t end   = rq->range_end;

    if (end > g.s_file_size) end = g.s_file_size;
    if (start >= end) return;

    uint32_t first_block = start / FS_DATA_LENGTH;
    uint32_t last_byte   = end - 1;
    uint32_t last_block  = last_byte / FS_DATA_LENGTH;

    FURI_LOG_I(TAG, "fs_handle_request: tx_id=%d, range_start=%lu, range_end=%lu, "
             "first_block=%lu, last_block=%lu",
             tx_id, start, end, first_block, last_block);

    g.s_is_blocks_requested = 1;
    g.s_block_needed_first = first_block;
    g.s_block_needed_last = last_block;

}

static void fs_handle_data(uint8_t tx_id, const FS_pl_data_t* d) {
    g.last_rx_ms = g.cb_now_ms ? g.cb_now_ms() : 0;

    // FURI_LOG_I(TAG, "fs_handle_data: tx_id=%d, block_number=%lu", tx_id, d->block_number);

    if (g.mode != FS_MODE_RECEIVER) return;
    if (!g.r_locked || tx_id != g.r_locked_tx_id) return;

    if (fs_map_get(d->block_number) == 1) { // already received
        FURI_LOG_W(TAG, "fs_handle_data: block %lu already received", d->block_number);
        return;
    }

    if (!g.cb_write_block) return;

    // Calculate valid block length based on file size
    uint32_t block_start = d->block_number * FS_DATA_LENGTH;
    if (block_start >= g.r_file_size) return; // out of range

    uint32_t remaining = g.r_file_size - block_start;
    uint32_t valid_len = (remaining >= FS_DATA_LENGTH) ? FS_DATA_LENGTH : remaining;

    g.cb_write_block(d->block_number, d->data, valid_len);

    fs_map_set(d->block_number, 1); // mark block as received
    fs_parts_on_block_set(d->block_number); // handle parts
    g.r_blocks_received++;

    FURI_LOG_I(TAG, "fs_handle_data[txid=%d]: block %lu written, valid_len=%lu, "
             "blocks_received: %lu/%lu", tx_id,
             d->block_number, valid_len, g.r_blocks_received, g.r_blocks_needed);

}

// Main entry for raw packets
void fs_receive_callback(const uint8_t* buf, size_t size) {
    FS_packet_t pkt;
    if (!fs_packet_check_and_parse(buf, size, &pkt)) {
        // FURI_LOG_E(TAG, "Failed: fs_packet_check_and_parse");
        return;
    }

    switch ((fs_pkt_type_t)pkt.packet_type) {
        case FS_PKT_ANNOUNCE: {
            FURI_LOG_I(TAG, "Received ANNOUNCE, tx_id %d", pkt.tx_id);
            fs_notify_led_blue();
            FS_pl_announce_t ann;
            fs_pl_announce_unpack(pkt.payload, &ann);
            fs_handle_announce(pkt.tx_id, &ann);
        } break;
        case FS_PKT_REQUEST: {
            FURI_LOG_I(TAG, "Received REQUEST, tx_id %d", pkt.tx_id);
            // fs_notify_led_green();
            FS_pl_request_t rq;
            fs_pl_request_unpack(pkt.payload, &rq);
            fs_handle_request(pkt.tx_id, &rq);
        } break;
        case FS_PKT_DATA: {
            // FURI_LOG_I(TAG, "Received DATA, tx_id %d", pkt.tx_id);
            fs_notify_led_green();
            FS_pl_data_t d;
            fs_pl_data_unpack(pkt.payload, &d);
            fs_handle_data(pkt.tx_id, &d);
        } break;
        default:
            fs_notify_led_red();
            FURI_LOG_E(TAG, "Unknown packet type: %d", pkt.packet_type);
            break;
    }
}

