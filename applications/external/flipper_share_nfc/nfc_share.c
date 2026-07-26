#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#include <applications/services/notification/notification.h> // NotificationApp
#include <notification/notification_messages.h>

#define TAG "FlipperShareNFC"

#include "nfc_share.h"
#include "nfc_transport.h"
#include "md5_hash.h"

#define NSH_RECEIVER_DIRECTORY "inbox"

// ===== Constants of protocol / timings =====
// Retuned for the NFC link: the poller drives a strict command/response exchange
// every few ms, one packet per exchange, so timeouts are short and there are no
// TX collisions to arbitrate.
enum {
    NSH_VERSION = 2, // wire format v2: variable-length packets, CRC16
    NSH_ANNOUNCE_INTERVAL_MS = 1000, // announce/discovery interval (idle sender)
    NSH_ANNOUNCE_CONNECTED_MS = 3000, // announce interval while serving a transfer.
        // We keep announcing during the stream (just
        // less often) so a receiver can be restarted
        // and re-lock without touching the sender.
    NSH_RX_TIMEOUT_MS = 500, // receiver re-request timeout. MUST exceed the full
        // REQUEST->first-DATA round trip (a few poll periods
        // plus the sender's TX gap, ~100-200ms total), else
        // the receiver re-requests before the first block
        // arrives. During streaming, blocks arrive every few
        // tens of ms << this, so no spurious re-requests.
    NSH_TX_TIMEOUT_MS = 50, // sender gap after last RX before streaming data
    NSH_IDLE_TICK_MS = 20, // interval for calling nsh_idle()
    NSH_CONNECTED_IDLE_MS = 5000, // sender reverts CONNECTED->ANNOUNCING after this idle
    NSH_REQUEST_JITTER_MS = 50 // small random backoff before each (re)REQUEST; NFC is
        // command/response, so this only desynchronizes retry
        // bursts, no ALOHA phase-lock to break (unlike IR)
};

// ===== Global state =====

nsh_ctx_t g; //static, extern in .h

// ===== Shared-state lock =====
// Protects `g`, g_map and nsh_parts against concurrent access from the worker
// thread (nsh_idle), the NFC RX-callback thread (nsh_handle_*) and the GUI
// timer/draw callbacks. Kept as a file-scope static (NOT a field of `g`, so
// memset(&g,0) in nsh_init does not clobber it).
static FuriMutex* g_lock = NULL;

void nsh_lock_ensure(void) {
    if(!g_lock) {
        g_lock = furi_mutex_alloc(FuriMutexTypeNormal);
    }
}

static void nsh_lock_free(void) {
    if(g_lock) {
        furi_mutex_free(g_lock);
        g_lock = NULL;
    }
}

void nsh_lock(void) {
    if(g_lock) furi_mutex_acquire(g_lock, FuriWaitForever);
}

void nsh_unlock(void) {
    if(g_lock) furi_mutex_release(g_lock);
}

bool nsh_try_lock_ms(uint32_t timeout_ms) {
    return g_lock && (furi_mutex_acquire(g_lock, furi_ms_to_ticks(timeout_ms)) == FuriStatusOk);
}

// ===== Chunked MD5 with progress =====
// Bytes hashed / total for the MD5 currently computed by
// nsh_md5_calc_file_progress (sender init and receiver finalization). File-scope
// statics (not fields of `g`) so memset(&g, 0) does not clobber a running hash.
static uint32_t g_hash_done = 0;
static uint32_t g_hash_total = 0;

bool nsh_hash_progress_get(uint32_t* done, uint32_t* total) {
    if(!nsh_try_lock_ms(10)) return false;
    *done = g_hash_done;
    *total = g_hash_total;
    nsh_unlock();
    return true;
}

// Same contract as toolbox md5_calc_file(), plus:
//  - publishes progress for the GUI after every NSH_HASH_CHUNK_SIZE bytes;
//  - checks the worker stop flag between chunks and aborts (returns false),
//    so Cancel/Back while hashing a big file does not block in thread join.
static bool nsh_md5_calc_file_progress(
    File* file,
    const char* path,
    unsigned char output[16],
    FS_Error* file_error) {
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file_error) *file_error = storage_file_get_error(file);
        return false;
    }

    nsh_lock();
    g_hash_done = 0;
    g_hash_total = storage_file_size(file);
    nsh_unlock();

    uint8_t* data = malloc(NSH_HASH_CHUNK_SIZE);
    Md5Context md5_ctx;
    md5_hash_init(&md5_ctx);

    bool result = true;
    while(true) {
        if(furi_thread_flags_get() & NSH_WORKER_STOP_FLAG) {
            result = false;
            break;
        }
        size_t read_size = storage_file_read(file, data, NSH_HASH_CHUNK_SIZE);
        if(storage_file_get_error(file) != FSE_OK) {
            result = false;
            break;
        }
        if(read_size == 0) break;
        md5_hash_update(&md5_ctx, data, read_size);
        nsh_lock();
        g_hash_done += read_size;
        nsh_unlock();
    }
    md5_hash_finish(&md5_ctx, output);
    free(data);

    if(file_error) *file_error = storage_file_get_error(file);
    storage_file_close(file);
    return result;
}

void nsh_notify_success(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_success);
    furi_record_close(RECORD_NOTIFICATION);
}

void nsh_notify_error(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_error);
    furi_record_close(RECORD_NOTIFICATION);
}

void nsh_notify_vibro(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_single_vibro);
    furi_record_close(RECORD_NOTIFICATION);
}

void nsh_notify_led_red(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_red_10);
    furi_record_close(RECORD_NOTIFICATION);
}

void nsh_notify_led_green(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_green_10);
    furi_record_close(RECORD_NOTIFICATION);
}

void nsh_notify_led_blue(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_blue_10);
    furi_record_close(RECORD_NOTIFICATION);
}

void nsh_notify_led_cyan(void) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_blink_cyan_10);
    furi_record_close(RECORD_NOTIFICATION);
}

// ===== Bit map for received blocks (1 bit per block) =====================

// Map
typedef struct {
    uint8_t* bits; // array of bytes, 1 bit = 1 block
    uint32_t count; // number of blocks (valid bits)
    uint32_t nbytes; // length of bits array in bytes
} nsh_blockmap_t;

static nsh_blockmap_t g_map = {0};

static inline uint32_t _nsh_div8(uint32_t x) {
    return x >> 3;
}
static inline uint32_t _nsh_mod8(uint32_t x) {
    return x & 7u;
}

bool nsh_map_init(uint32_t block_count) { // default all blocks = 0 (means not received)
    if(g_map.bits) {
        free(g_map.bits);
        g_map.bits = NULL;
    }
    g_map.count = 0;
    g_map.nbytes = 0;

    if(block_count == 0) return true; // empty map is allowed

    uint32_t nbytes = (block_count + 7u) / 8u; // calculate buffer size and check for overflow

    // simple overflow check for size_t in malloc
    if(nbytes == 0 || nbytes > (uint32_t)SIZE_MAX) return false;

    uint8_t* buf = (uint8_t*)malloc(nbytes);
    if(!buf) return false;

    memset(buf, 0x00, nbytes);

    // Make tail bits (beyond count) = 1, to avoid false positives in zero search
    uint32_t rem = block_count & 7u; // count % 8
    if(rem != 0) {
        uint8_t tail_mask = (uint8_t) ~((1u << rem) - 1u); // bits beyond count
        buf[nbytes - 1] |= tail_mask;
    }

    g_map.bits = buf;
    g_map.count = block_count;
    g_map.nbytes = nbytes;
    return true;
}

void nsh_map_deinit(void) {
    if(g_map.bits) {
        free(g_map.bits);
        g_map.bits = NULL;
    }
    g_map.count = 0;
    g_map.nbytes = 0;
}

bool nsh_map_set(
    uint32_t block_number,
    uint8_t value) { // Set numbered block to 0/1, return false if out of range
    if(!g_map.bits || block_number >= g_map.count) return false;
    uint32_t i = _nsh_div8(block_number);
    uint8_t m = (uint8_t)(1u << _nsh_mod8(block_number));
    if(value)
        g_map.bits[i] |= m;
    else
        g_map.bits[i] &= (uint8_t)~m;
    return true;
}

int nsh_map_get(uint32_t block_number) { // Get numbered block value 0/1, -1 if out of range
    if(!g_map.bits || block_number >= g_map.count) return -1;
    uint32_t i = _nsh_div8(block_number);
    uint8_t m = (uint8_t)(1u << _nsh_mod8(block_number));
    return (g_map.bits[i] & m) ? 1 : 0;
}

// Find first num with value 0/1, starting from offset_from (inclusive). Returns UINT32_MAX if not found
uint32_t nsh_map_search(uint8_t bitval, uint32_t offset_from) {
    if(!g_map.bits) return UINT32_MAX;
    if(offset_from >= g_map.count) return UINT32_MAX;

    uint32_t byte_idx = _nsh_div8(offset_from);
    uint32_t bit_off = _nsh_mod8(offset_from);

    // first (partial) iteration — mask bits before offset_from
    {
        uint8_t byte = g_map.bits[byte_idx];
        if(bitval == 0) byte = (uint8_t)~byte; // searching for zeros => invert

        // mask: keep bits [bit_off..7]
        uint8_t mask = (uint8_t)(0xFFu << bit_off);
        uint8_t cand = (uint8_t)(byte & mask);
        if(cand) {
            // find first set bit
            for(uint32_t b = bit_off; b < 8; ++b) {
                if(cand & (1u << b)) {
                    uint32_t idx = (byte_idx << 3) + b;
                    if(idx < g_map.count)
                        return idx;
                    else
                        return UINT32_MAX;
                }
            }
        }
        byte_idx++;
    }

    // full bytes
    for(; byte_idx < g_map.nbytes; ++byte_idx) {
        uint8_t byte = g_map.bits[byte_idx];
        if(bitval == 0) byte = (uint8_t)~byte;

        if(byte) { // if it is last byte, protect against overflow count
            bool last = (byte_idx == g_map.nbytes - 1);
            for(uint32_t b = 0; b < 8; ++b) {
                if(byte & (1u << b)) {
                    uint32_t idx = (byte_idx << 3) + b;
                    if(!last || idx < g_map.count)
                        return idx;
                    else
                        return UINT32_MAX;
                }
            }
        }
    }

    return UINT32_MAX;
}

bool nsh_map_all_set(void) { // Quick check if all blocks received, true if all bits is 1
    if(!g_map.bits) return false;
    // all intermediate bytes must be 0xFF
    for(uint32_t i = 0; i + 1 < g_map.nbytes; ++i) {
        if(g_map.bits[i] != 0xFFu) return false;
    }
    // last byte must also be 0xFF, since we set "tail" bits to 1 during init
    return g_map.bits[g_map.nbytes - 1] == 0xFFu;
}

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). A per-packet CRC8 could let a
// corrupted packet slip through (~1/256), get written as a good block and doom
// the whole transfer to an end-of-file MD5 failure with no localized recovery.
// CRC16 makes that ~1/65536, so the ARQ actually converges "until success".
static uint16_t nsh_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for(int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

typedef enum {
    NSH_PARTS_MODE_NONE = 0,
    NSH_PARTS_MODE_BLOCKS_PER_PART, // B >= N
    NSH_PARTS_MODE_PARTS_PER_BLOCK // B <  N
} nsh_parts_mode_t;

static struct {
    uint32_t B; // block_count
    uint32_t N; // parts count (= NSH_PARTS_COUNT)
    nsh_parts_mode_t mode;
    uint32_t received[NSH_PARTS_COUNT]; // blocks received per part
    uint32_t total[NSH_PARTS_COUNT]; // blocks total per part (>=1 unless B==0)
} nsh_parts;

static inline uint32_t _scale_floor_u32(uint32_t x, uint32_t mul, uint32_t den) {
    return (uint32_t)(((uint64_t)x * (uint64_t)mul) / (uint64_t)den);
}

// ceil(x*mul/den)
static inline uint32_t _scale_ceil_u32(uint32_t x, uint32_t mul, uint32_t den) {
    return (uint32_t)(((uint64_t)x * (uint64_t)mul + (uint64_t)den - 1u) / (uint64_t)den);
}

// Pre-calc per-part totals when B>=N. Part s owns blocks [ceil(s*B/N), ceil((s+1)*B/N)),
// which is the exact inverse of part(i)=floor(i*N/B) used in nsh_parts_on_block_set.
// => sum(total)=B and each part fills exactly when all its blocks arrive (no
// floor/ceil mismatch, so no "never-filling" parts).
static void _init_blocks_per_part(void) {
    const uint32_t B = nsh_parts.B;
    const uint32_t N = nsh_parts.N;

    uint32_t prev = 0; // ceil(0*B/N)
    for(uint32_t s = 0; s < N; ++s) {
        uint32_t next = _scale_ceil_u32(s + 1, B, N);
        nsh_parts.total[s] = next - prev; // >=1 since B>=N
        nsh_parts.received[s] = 0;
        prev = next;
    }
}

bool nsh_parts_init(uint32_t block_count) {
    memset(&nsh_parts, 0, sizeof(nsh_parts));
    nsh_parts.N = NSH_PARTS_COUNT;
    nsh_parts.B = block_count;

    if(nsh_parts.N == 0) {
        nsh_parts.mode = NSH_PARTS_MODE_NONE;
        return false;
    }

    if(nsh_parts.B == 0) {
        nsh_parts.mode = NSH_PARTS_MODE_NONE;
        return true;
    }

    if(nsh_parts.B >= nsh_parts.N) {
        nsh_parts.mode = NSH_PARTS_MODE_BLOCKS_PER_PART;
        _init_blocks_per_part();
    } else {
        // B<N: each part is covered by exactly one block → total 1 per part.
        nsh_parts.mode = NSH_PARTS_MODE_PARTS_PER_BLOCK;
        for(uint32_t s = 0; s < nsh_parts.N; ++s) {
            nsh_parts.total[s] = 1u;
            nsh_parts.received[s] = 0u;
        }
    }
    return true;
}

void nsh_parts_deinit(void) {
    memset(&nsh_parts, 0, sizeof(nsh_parts));
}

void nsh_parts_on_block_set(uint32_t i) {
    if(nsh_parts.B == 0 || nsh_parts.N == 0 || nsh_parts.mode == NSH_PARTS_MODE_NONE) return;
    if(i >= nsh_parts.B) return; // out of range

    if(nsh_parts.mode == NSH_PARTS_MODE_BLOCKS_PER_PART) {
        // Block i belongs to exactly one part: part(i) = floor(i*N/B).
        uint32_t s = _scale_floor_u32(i, nsh_parts.N, nsh_parts.B);
        if(s >= nsh_parts.N) return; // bounds check

        // Count it (idempotent to duplicate DATA — capped at total[s]).
        if(nsh_parts.received[s] < nsh_parts.total[s]) {
            nsh_parts.received[s]++;
        }
    } else {
        // NSH_PARTS_MODE_PARTS_PER_BLOCK (B<N): block covers range of parts [sf .. sl]
        // sf = floor(i*N/B), sl = floor((i+1)*N/B) - 1
        uint32_t sf = _scale_floor_u32(i, nsh_parts.N, nsh_parts.B);
        uint32_t sl = _scale_floor_u32(i + 1, nsh_parts.N, nsh_parts.B);
        if(sl > 0) sl -= 1u;

        if(sf >= nsh_parts.N) return;
        if(sl >= nsh_parts.N) sl = nsh_parts.N - 1u;
        if(sf > sl) return;

        // Mark covered columns full (total[s]==1). Idempotent.
        for(uint32_t s = sf; s <= sl; ++s) {
            nsh_parts.received[s] = nsh_parts.total[s];
        }
    }
}

int nsh_parts_get(uint32_t part_index) {
    if(part_index >= nsh_parts.N || nsh_parts.N == 0) return -1;
    return (nsh_parts.total[part_index] > 0 &&
            nsh_parts.received[part_index] >= nsh_parts.total[part_index]) ?
               1 :
               0;
}

// Fill dst[0..NSH_PARTS_COUNT-1] with per-part fill level 0..255 (received/total).
// A partially-received part yields >=1 so early progress is visible; a fully
// received part yields 255. Caller must hold g_lock.
void nsh_parts_levels_copy(uint8_t* dst) {
    if(!dst) return;
    for(uint32_t s = 0; s < NSH_PARTS_COUNT; ++s) {
        uint32_t tot = nsh_parts.total[s];
        uint32_t rcv = nsh_parts.received[s];
        if(tot == 0 || rcv == 0) {
            dst[s] = 0;
        } else if(rcv >= tot) {
            dst[s] = 255;
        } else {
            uint32_t v = (uint32_t)(((uint64_t)rcv * 255u) / tot);
            dst[s] = (uint8_t)(v ? v : 1u);
        }
    }
}

uint32_t nsh_parts_count(void) {
    return nsh_parts.N;
}

uint32_t nsh_parts_block_count(void) {
    return nsh_parts.B;
}

bool nsh_parts_is_ready(void) {
    // Assume "ready" if N>0 and (B==0 or some mode selected)
    if(nsh_parts.N == 0) return false;
    if(nsh_parts.B == 0) return true;
    return nsh_parts.mode == NSH_PARTS_MODE_BLOCKS_PER_PART ||
           nsh_parts.mode == NSH_PARTS_MODE_PARTS_PER_BLOCK;
}

// ===== Helpers =====

// Adaptive duration: "M:SS" under 1h, "H:MM:SS" from 1h. Divisors are constants.
void nsh_fmt_duration(uint32_t secs, char* buf, size_t n) {
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    uint32_t s = secs % 60;
    if(h) {
        snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    } else {
        snprintf(buf, n, "%lu:%02lu", (unsigned long)m, (unsigned long)s);
    }
}

const char* nsh_basename(const char* path) {
    if(!path || !*path) return ""; // empty

    const char* end = path + strlen(path); // end points to '\0'

    while(end > path && end[-1] == '/')
        end--; // remove trailing '/'

    if(end == path) return "/"; // string consisted only of '/' (e.g. "/","///")

    const char* p = end;
    while(p > path && p[-1] != '/')
        p--; // find last '/'

    return p; // [p, end) — basename
}

void nsh_hexdump(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    for(size_t i = 0; i < len; ++i) {
        if((i % 16) == 0) printf("%04zu: ", i);
        printf("%02X ", p[i]);
        if((i % 16) == 15 || i + 1 == len) printf("\n");
    }
}

// ===== Serialization/Deserialization of Packets =====
// Wire layout: [version][tx_id][packet_type][payload...][crc16 lo][crc16 hi].
// The payload length depends on the packet type (control vs data), which lets a
// small DATA size coexist with a control packet big enough to hold the file name.

static inline size_t nsh_payload_len_for_type(uint8_t pkt_type) {
    return (pkt_type == NSH_PKT_DATA) ? NSH_DATA_PAYLOAD_LENGTH : NSH_CTRL_PAYLOAD_LENGTH;
}

static inline size_t nsh_packet_len_for_type(uint8_t pkt_type) {
    return NSH_HEADER_LENGTH + nsh_payload_len_for_type(pkt_type) + NSH_CRC_LENGTH;
}

// Serialize into out (>= NSH_PACKET_MAX). payload_src must hold at least
// nsh_payload_len_for_type(pkt_type) bytes. Returns the total packet length.
static size_t nsh_packet_pack(
    uint8_t* out,
    uint8_t version,
    uint8_t tx_id,
    uint8_t pkt_type,
    const uint8_t* payload_src) {
    const size_t pl = nsh_payload_len_for_type(pkt_type);
    out[0] = version;
    out[1] = tx_id;
    out[2] = pkt_type;
    memcpy(out + NSH_HEADER_LENGTH, payload_src, pl);
    uint16_t crc = nsh_crc16(out, NSH_HEADER_LENGTH + pl);
    out[NSH_HEADER_LENGTH + pl] = (uint8_t)(crc & 0xFF);
    out[NSH_HEADER_LENGTH + pl + 1] = (uint8_t)(crc >> 8);
    return NSH_HEADER_LENGTH + pl + NSH_CRC_LENGTH;
}

// Validate a received packet. On success returns true and sets *tx_id, *pkt_type
// and *payload (points into buf). Rejects bad version, unknown type, wrong
// length-for-type, or CRC mismatch.
static bool nsh_packet_check_and_parse(
    const uint8_t* buf,
    size_t len,
    uint8_t* tx_id,
    uint8_t* pkt_type,
    const uint8_t** payload) {
    if(len < NSH_HEADER_LENGTH + NSH_CRC_LENGTH) return false;
    if(buf[0] != NSH_VERSION) return false;
    uint8_t type = buf[2];
    if(type != NSH_PKT_ANNOUNCE && type != NSH_PKT_REQUEST && type != NSH_PKT_DATA) return false;
    if(len != nsh_packet_len_for_type(type)) return false;

    const size_t pl = nsh_payload_len_for_type(type);
    uint16_t calc = nsh_crc16(buf, NSH_HEADER_LENGTH + pl);
    uint16_t got = (uint16_t)buf[NSH_HEADER_LENGTH + pl] |
                   ((uint16_t)buf[NSH_HEADER_LENGTH + pl + 1] << 8);
    if(calc != got) return false;

    *tx_id = buf[1];
    *pkt_type = type;
    *payload = buf + NSH_HEADER_LENGTH;
    return true;
}

// ===== Payloads: pack/unpack =====

// ANNOUNCE (control payload)
static void nsh_pl_announce_pack(uint8_t* payload_out, const NSH_pl_announce_t* pl_src) {
    memcpy(payload_out, pl_src->file_name, NSH_FILENAME_LENGTH);
    memcpy(payload_out + NSH_FILENAME_LENGTH, &pl_src->file_size, sizeof(uint32_t)); // LE
    memcpy(payload_out + NSH_FILENAME_LENGTH + 4, pl_src->hash_md5, 16);
}

static void nsh_pl_announce_unpack(const uint8_t* payload, NSH_pl_announce_t* pl_out) {
    memcpy(pl_out->file_name, payload, NSH_FILENAME_LENGTH);
    memcpy(&pl_out->file_size, payload + NSH_FILENAME_LENGTH, sizeof(uint32_t));
    memcpy(pl_out->hash_md5, payload + NSH_FILENAME_LENGTH + 4, 16);
}

// REQUEST (control payload; unused bytes zero-padded)
static void nsh_pl_request_pack(uint8_t* payload_out, const NSH_pl_request_t* pl_src) {
    memset(payload_out, 0, NSH_CTRL_PAYLOAD_LENGTH);
    memcpy(payload_out + 0, &pl_src->range_start, sizeof(uint32_t));
    memcpy(payload_out + 4, &pl_src->range_end, sizeof(uint32_t));
}

static void nsh_pl_request_unpack(const uint8_t* payload, NSH_pl_request_t* pl_out) {
    memcpy(&pl_out->range_start, payload + 0, sizeof(uint32_t));
    memcpy(&pl_out->range_end, payload + 4, sizeof(uint32_t));
}

// DATA (data payload = block_number + NSH_DATA_LENGTH bytes)
static void nsh_pl_data_pack(uint8_t* payload_out, const NSH_pl_data_t* pl_src) {
    memcpy(payload_out + 0, &pl_src->block_number, sizeof(uint32_t));
    memcpy(payload_out + 4, pl_src->data, NSH_DATA_LENGTH);
}

static void nsh_pl_data_unpack(const uint8_t* payload, NSH_pl_data_t* pl_out) {
    memcpy(&pl_out->block_number, payload + 0, sizeof(uint32_t));
    memcpy(pl_out->data, payload + 4, NSH_DATA_LENGTH);
}

// ===== API Implementation =====

void nsh_ensure_inbox_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;

    bool ok = storage_simply_mkdir(storage, EXT_PATH(NSH_RECEIVER_DIRECTORY));
    if(!ok) {
        FURI_LOG_E(
            TAG,
            "nsh_ensure_inbox_dir: Failed to create directory '%s'",
            EXT_PATH(NSH_RECEIVER_DIRECTORY));
    }
    // TODO: check return value?

    furi_record_close(RECORD_STORAGE);
}

// Close the persistent session file handle (receiver write / sender read) if
// open, and release the storage record. Caller MUST hold g_lock. Idempotent.
static void nsh_close_session_file(void) {
    if(g.file) {
        storage_file_close(g.file);
        storage_file_free(g.file);
        g.file = NULL;
    }
    if(g.storage) {
        furi_record_close(RECORD_STORAGE);
        g.storage = NULL;
    }
}

// Create/truncate the receive file and KEEP it open for the whole session in
// g.storage/g.file (#3: no more open/close per block). Preallocates the cluster
// chain by seeking to file_size (write-mode seek past EOF extends the file),
// then rewinds to 0. Caller holds g_lock (nsh_handle_announce).
uint8_t nsh_file_create_truncate(uint32_t file_size) {
    FURI_LOG_I(TAG, "nsh_file_create_truncate: Creating and truncating file '%s'", g.r_file_path);
    if(g.mode != NSH_MODE_RECEIVER) {
        FURI_LOG_E(TAG, "nsh_file_create_truncate: Not available in SENDER mode");
        return 1;
    }

    // Defensive: drop any leftover handle from a previous session.
    nsh_close_session_file();

    g.storage = furi_record_open(RECORD_STORAGE);
    g.file = storage_file_alloc(g.storage);
    if(!g.file) {
        FURI_LOG_E(TAG, "nsh_file_create_truncate: Failed to allocate file handle");
        nsh_close_session_file();
        return 2;
    }
    if(!storage_file_open(g.file, g.r_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "nsh_file_create_truncate: Failed to open file '%s'", g.r_file_path);
        nsh_close_session_file();
        return 3;
    }

    // Preallocate the full file, then rewind. Per-block writes will seek in place.
    if(!storage_file_seek(g.file, file_size, true)) {
        FURI_LOG_E(TAG, "nsh_file_create_truncate: Failed to seek to offset %lu", file_size);
        nsh_close_session_file();
        return 4;
    }
    storage_file_seek(g.file, 0, true);

    return 0; // handle stays open in g.file for the session
}

// Called from nsh_handle_data (RX thread) while holding g_lock. Writes into the
// persistent session handle (opened once in nsh_file_create_truncate). FatFS
// coalesces sub-sector writes in the FIL's 512-byte buffer, so physical SD
// writes happen ~once per 512 bytes, not per block.
void my_write_block(uint32_t block_number, const uint8_t in[NSH_DATA_LENGTH], uint32_t valid_len) {
    if(!g.file) {
        FURI_LOG_E(TAG, "my_write_block: no open file handle");
        return;
    }
    uint32_t offset = block_number * NSH_DATA_LENGTH;

    if(!storage_file_seek(g.file, offset, true)) {
        FURI_LOG_E(TAG, "my_write_block: Failed to seek to offset %lu", offset);
        return;
    }

    uint32_t bytes_written = storage_file_write(g.file, in, valid_len);
    if(bytes_written < valid_len) {
        FURI_LOG_W(TAG, "my_write_block: Write error, only %lu bytes written", bytes_written);
    }
}

// Sender-side block read from the persistent session handle (opened once in
// nsh_init). Called from nsh_send_data on the worker thread under g_lock.
uint32_t my_read_block(uint32_t block_number, uint8_t out[NSH_DATA_LENGTH]) {
    memset(out, 0, NSH_DATA_LENGTH);
    if(!g.file) {
        FURI_LOG_E(TAG, "my_read_block: no open file handle");
        return 0;
    }

    storage_file_seek(g.file, block_number * NSH_DATA_LENGTH, true);

    uint32_t bytes = storage_file_read(g.file, out, NSH_DATA_LENGTH);
    if(bytes < NSH_DATA_LENGTH) {
        FURI_LOG_W(TAG, "my_read_block: Read error, last block? %lu bytes read", bytes);
    }

    return bytes; // number of valid bytes read, can be < NSH_DATA_LENGTH for last block
}

bool nsh_init_from_external_transmit(const char* file_path) {
    // FURI_LOG_I(TAG, "nsh_init_from_external_transmit: file_path='%s'", file_path);

    nsh_init_params_t ps;
    memset(&ps, 0, sizeof(ps));
    ps.mode = NSH_MODE_SENDER;
    ps.tx_id = (uint8_t)(furi_get_tick() & 0xFFu);
    ps.send_bytes = nfc_transport_send;
    ps.now_ms = furi_get_tick;

    if(file_path) {
        strncpy(ps.s_file_path, file_path, sizeof(ps.s_file_path) - 1);
        ps.s_file_path[sizeof(ps.s_file_path) - 1] = '\0';
    } else {
        ps.s_file_path[0] = '\0';
    }
    ps.s_read_block = my_read_block;
    return nsh_init(&ps);
}

bool nsh_init_from_external_receive() {
    nsh_init_params_t pr = {
        .mode = NSH_MODE_RECEIVER,
        .tx_id = 0x00,
        .send_bytes = nfc_transport_send,
        .now_ms = furi_get_tick,
        .r_write_block = my_write_block};
    return nsh_init(&pr);
}

bool nsh_init(const nsh_init_params_t* p) {
    if(!p || !p->send_bytes || !p->now_ms) {
        FURI_LOG_E(TAG, "nsh_init: Invalid parameters");
        return false;
    }
    if(p->mode != NSH_MODE_SENDER && p->mode != NSH_MODE_RECEIVER) {
        FURI_LOG_E(TAG, "nsh_init: Invalid mode %d", p->mode);
        return false;
    }

    // Make sure the shared-state lock exists (normally created in the scene
    // on_enter before any thread starts; this is a defensive fallback).
    nsh_lock_ensure();

    if(p->mode == NSH_MODE_SENDER) {
        FURI_LOG_I(TAG, "nsh_init: SENDER mode, file_path='%s'", p->s_file_path);

        if(!p->s_read_block) return false;
        if(p->s_file_path[0] == '\0') {
            FURI_LOG_E(TAG, "nsh_init: Invalid file path");
            return false;
        }

        const char* basename = nsh_basename(p->s_file_path);
        if(strlen(basename) >= NSH_FILENAME_LENGTH) {
            FURI_LOG_E(TAG, "nsh_init: File name too long: %s", basename);
            return false;
        }

        // Read size + compute MD5 on LOCAL handles (no shared g.storage/g.file),
        // BEFORE touching `g`. nsh_md5_calc_file_progress() opens the file by path itself.
        uint32_t file_size = 0;
        unsigned char md5[16];
        memset(md5, 0, sizeof(md5));

        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);
        if(!file) {
            FURI_LOG_E(TAG, "nsh_init: Failed to allocate file handle");
            furi_record_close(RECORD_STORAGE);
            return false;
        }
        if(!storage_file_open(file, p->s_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "nsh_init: Failed to open file '%s'", p->s_file_path);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }
        file_size = storage_file_size(file);
        storage_file_close(file);

        FS_Error err = 0;
        bool md5_ok = nsh_md5_calc_file_progress(file, p->s_file_path, md5, &err);

        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        // Abort only on cancellation; a plain read error keeps the old
        // md5_calc_file behavior (proceed, receiver detects the mismatch).
        if(!md5_ok && (furi_thread_flags_get() & NSH_WORKER_STOP_FLAG)) {
            FURI_LOG_I(TAG, "nsh_init: hashing cancelled");
            return false;
        }

        // #3: open ONE persistent read handle for the whole session. Done AFTER
        // md5 released its handle on the same path (per-path FSE_ALREADY_OPEN).
        Storage* psto = furi_record_open(RECORD_STORAGE);
        File* pfile = storage_file_alloc(psto);
        if(!pfile || !storage_file_open(pfile, p->s_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(
                TAG, "nsh_init: Failed to open persistent read handle '%s'", p->s_file_path);
            if(pfile) storage_file_free(pfile);
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        // Publish the fully-prepared session state atomically.
        nsh_lock();
        memset(&g, 0, sizeof(g));
        g.mode = NSH_MODE_SENDER;
        g.state = NSH_ST_ANNOUNCING;
        g.cb_send_bytes = p->send_bytes;
        g.cb_now_ms = p->now_ms;
        g.last_tick_ms = g.last_announce_ms = g.last_rx_ms = p->now_ms();
        g.tx_id = p->tx_id;
        strncpy(g.s_file_path, p->s_file_path, sizeof(g.s_file_path) - 1);
        g.s_file_path[sizeof(g.s_file_path) - 1] = '\0';
        strncpy(g.s_file_name, basename, NSH_FILENAME_LENGTH - 1);
        g.s_file_name[NSH_FILENAME_LENGTH - 1] = '\0';
        g.s_file_size = file_size;
        memcpy(g.s_md5, md5, sizeof(g.s_md5));
        g.cb_read_block = p->s_read_block;
        g.s_is_blocks_requested = 0;
        g.s_block_needed_first = 0;
        g.s_block_needed_last = 0;
        g.storage = psto; // persistent read handle
        g.file = pfile;
        nsh_unlock();

        FURI_LOG_I(
            TAG,
            "nsh_init: SENDER, file_path='%s', file_name='%s', file_size=%lu, tx_id=%d",
            g.s_file_path,
            g.s_file_name,
            g.s_file_size,
            g.tx_id);
        FURI_LOG_I(
            TAG,
            "nsh_init: md5=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            g.s_md5[0],
            g.s_md5[1],
            g.s_md5[2],
            g.s_md5[3],
            g.s_md5[4],
            g.s_md5[5],
            g.s_md5[6],
            g.s_md5[7],
            g.s_md5[8],
            g.s_md5[9],
            g.s_md5[10],
            g.s_md5[11],
            g.s_md5[12],
            g.s_md5[13],
            g.s_md5[14],
            g.s_md5[15]);

    } else { // RECEIVER:
        if(!p->r_write_block) return false;

        nsh_lock();
        memset(&g, 0, sizeof(g));
        g.mode = NSH_MODE_RECEIVER;
        g.state = NSH_ST_RECEIVING;
        g.cb_send_bytes = p->send_bytes;
        g.cb_now_ms = p->now_ms;
        g.last_tick_ms = g.last_announce_ms = g.last_rx_ms = p->now_ms();
        g.cb_write_block = p->r_write_block;
        g.tx_id = 0; // will be set from ANNOUNCE
        nsh_unlock();

        FURI_LOG_I(TAG, "nsh_init: RECEIVER");
    }
    return true;
}

// Must be called only after ALL threads that touch `g` are stopped/joined
// (worker thread + NFC stack). Frees the block map/parts, zeroes the
// context and finally destroys the shared-state lock.
void nsh_deinit(void) {
    FURI_LOG_I(TAG, "nsh_deinit");

    nsh_lock();
    // close the persistent session file handle if a transfer was interrupted
    // (on success the finalization stage already closed it); flushes the buffer
    nsh_close_session_file();

    // free block map / parts
    nsh_map_deinit();
    nsh_parts_deinit();

    // clear callbacks and other pointers to avoid use-after-free
    g.cb_send_bytes = NULL;
    g.cb_now_ms = NULL;
    g.cb_read_block = NULL;
    g.cb_write_block = NULL;

    // zero whole context after resources freed
    memset(&g, 0, sizeof(g));

    // reset hash progress so the next session's GUI doesn't see stale values
    g_hash_done = 0;
    g_hash_total = 0;
    nsh_unlock();

    // destroy the lock last, once no thread can use it anymore
    nsh_lock_free();

    FURI_LOG_I(TAG, "nsh_deinit: done");
}

// High-level sender helper functions

void nsh_send_announce(void) {
    if(g.mode != NSH_MODE_SENDER || !g.cb_send_bytes) return;

    FURI_LOG_I(
        TAG, "nsh_send_announce: file_name='%s', file_size=%lu", g.s_file_name, g.s_file_size);

    nsh_notify_led_blue();

    NSH_pl_announce_t pl = {0};
    memcpy(pl.file_name, g.s_file_name, NSH_FILENAME_LENGTH);
    pl.file_size = g.s_file_size;
    memcpy(pl.hash_md5, g.s_md5, 16);

    uint8_t payload[NSH_PAYLOAD_MAX];
    nsh_pl_announce_pack(payload, &pl);

    uint8_t pkt[NSH_PACKET_MAX];
    size_t n = nsh_packet_pack(pkt, NSH_VERSION, g.tx_id, NSH_PKT_ANNOUNCE, payload);
    g.cb_send_bytes(pkt, n);
}

void nsh_send_request(uint32_t range_start, uint32_t range_end) {
    // FURI_LOG_I(TAG, "nsh_send_request: range_start=%lu, range_end=%lu", range_start, range_end);

    if(g.mode != NSH_MODE_RECEIVER || !g.cb_send_bytes) return;

    // RECOMMENDATION: align range by NSH_DATA_LENGTH, except the tail == file_size
    NSH_pl_request_t pl = {.range_start = range_start, .range_end = range_end};
    uint8_t payload[NSH_PAYLOAD_MAX];
    nsh_pl_request_pack(payload, &pl);

    uint8_t pkt[NSH_PACKET_MAX];
    size_t n = nsh_packet_pack(pkt, NSH_VERSION, g.tx_id, NSH_PKT_REQUEST, payload);
    g.cb_send_bytes(pkt, n);
}

void nsh_send_data() {
    if(g.mode != NSH_MODE_SENDER || !g.cb_send_bytes || !g.cb_read_block) return;

    // Pick the next block to send under the lock (nsh_handle_request, on the RX
    // thread, mutates these fields concurrently).
    nsh_lock();
    if(g.s_is_blocks_requested == 0) {
        nsh_unlock();
        FURI_LOG_E(TAG, "nsh_send_data: No blocks needed, cannot send data");
        return;
    }
    uint32_t block_number = g.s_block_needed_first;
    if(g.s_block_needed_first < g.s_block_needed_last) {
        g.s_block_needed_first++;
    } else {
        g.s_is_blocks_requested = 0; // all requested blocks have been sent
    }
    nsh_unlock();

    FURI_LOG_I(TAG, "nsh_send_data: block_number=%lu", block_number);

    uint8_t data52[NSH_DATA_LENGTH];
    uint32_t valid =
        g.cb_read_block(block_number, data52); // fill and zero‑pad if valid<NSH_DATA_LENGTH

    // blink only each Nth block to avoid excessive LED activity on a fast transfer
    if(block_number % 10 == 0) nsh_notify_led_green();

    (void)valid; // receiver computes final length using file_size

    NSH_pl_data_t pl = {0};
    pl.block_number = block_number;
    memcpy(pl.data, data52, NSH_DATA_LENGTH);

    uint8_t payload[NSH_PAYLOAD_MAX];
    nsh_pl_data_pack(payload, &pl);

    uint8_t pkt[NSH_PACKET_MAX];
    size_t n = nsh_packet_pack(pkt, NSH_VERSION, g.tx_id, NSH_PKT_DATA, payload);
    g.cb_send_bytes(pkt, n);
}

// ===== Simple timer/behavior logic =====

void nsh_idle(void) {
    if(!g.cb_now_ms) return;
    uint32_t now = g.cb_now_ms();

    if(g.mode == NSH_MODE_SENDER) {
        // Decide what to do under the lock (last_rx_ms / s_is_blocks_requested are
        // mutated by nsh_handle_request on the RX thread), then act outside it —
        // the radio sends are slow and must not hold the lock.
        nsh_lock();
        // After a long idle (transfer finished / link dropped) go back to the
        // faster discovery announce rate. Never changes state mid-stream
        // (s_is_blocks_requested==1).
        if(g.state == NSH_ST_CONNECTED && g.s_is_blocks_requested == 0 &&
           (now - g.last_rx_ms > NSH_CONNECTED_IDLE_MS)) {
            g.state = NSH_ST_ANNOUNCING;
        }
        // Keep announcing throughout — a receiver can be restarted and re-lock
        // without touching the sender — just at a lower rate while actively serving
        // a transfer (CONNECTED), to cut the in-stream announce overhead.
        uint32_t ann_iv = (g.state == NSH_ST_CONNECTED) ? NSH_ANNOUNCE_CONNECTED_MS :
                                                          NSH_ANNOUNCE_INTERVAL_MS;
        bool do_announce = (now - g.last_announce_ms >= ann_iv);
        if(do_announce) g.last_announce_ms = now;
        bool do_data = (now - g.last_rx_ms > NSH_TX_TIMEOUT_MS) && (g.s_is_blocks_requested == 1);
        // Snapshot for the periodic diagnostic log below (state lives in `g`).
        int dbg_state = (int)g.state;
        uint8_t dbg_req = g.s_is_blocks_requested;
        uint32_t dbg_first = g.s_block_needed_first, dbg_last = g.s_block_needed_last;
        uint32_t dbg_rx_age = now - g.last_rx_ms;
        g.last_tick_ms = now;
        nsh_unlock();

        // Once/sec sender status for serial-log diagnosis of stuck transfers.
        static uint32_t s_dbg_last_ms = 0;
        if(now - s_dbg_last_ms >= 1000u) {
            s_dbg_last_ms = now;
            FURI_LOG_I(
                TAG,
                "sender: state=%d req=%u blk=%lu..%lu rx_age=%lums",
                dbg_state,
                dbg_req,
                dbg_first,
                dbg_last,
                dbg_rx_age);
        }

        if(do_announce) nsh_send_announce();
        if(do_data) nsh_send_data(); // takes the lock internally for bookkeeping
        return;
    }

    if(g.mode == NSH_MODE_RECEIVER) {
        nsh_lock();
        if(g.r_is_finished) {
            g.last_tick_ms = now;
            nsh_unlock();
            return;
        }

        // --- Decide whether to (re)send a REQUEST for the next missing window ---
        bool do_request = false;
        uint32_t nbyte_start = 0, nbyte_end = 0;
        if(now - g.last_rx_ms > NSH_RX_TIMEOUT_MS) {
            if(g.r_locked && !g.r_finalizing && g.r_blocks_received < g.r_blocks_needed) {
                uint32_t nblk_start = nsh_map_search(0, 0);
                uint32_t nblk_end = nsh_map_search(1, nblk_start + 1);
                nbyte_start = nblk_start * NSH_DATA_LENGTH;
                if(nblk_end == UINT32_MAX) {
                    nblk_end = g.r_blocks_needed - 1; // last missing block -> request to end
                }
                // No cap: request the whole first contiguous missing run. On a fresh
                // transfer that is the entire file, so ONE REQUEST makes the sender
                // stream to the end (with interleaved announces); afterwards the
                // receiver only re-requests genuinely missing (lost) blocks.
                nbyte_end = (nblk_end + 1) * NSH_DATA_LENGTH; // inclusive end
                do_request = (nblk_start != UINT32_MAX);
                FURI_LOG_I(
                    TAG,
                    "nsh_idle: REQUEST blocks (%lu, %lu), bytes (%lu, %lu)",
                    nblk_start,
                    nblk_end,
                    nbyte_start,
                    nbyte_end);
            }
            g.last_rx_ms = now; // debounce timer
        }

        // --- Detect completion and enter finalization ATOMICALLY ---
        // Setting r_finalizing (and clearing r_locked) under the lock makes
        // nsh_handle_announce/nsh_handle_data early-return, so nothing truncates or
        // writes the file while we hash it. r_file_path / r_md5 stay frozen.
        bool do_finalize = g.r_locked && !g.r_finalizing &&
                           (g.r_blocks_received == g.r_blocks_needed);
        if(do_finalize) {
            FURI_LOG_I(TAG, "nsh_idle: ALL BLOCKS RECEIVED, finalizing");
            g.r_finalizing = true;
            g.r_finnsh_ms = now; // reception end (all blocks in) — before MD5
            g.r_locked = false; // keep existing UX (lock released on completion)
            g.r_locked_tx_id = 0;
            g.r_blocks_received = 0;
            // Close the persistent write handle BEFORE hashing: flushes the last
            // partial sector + dir-entry and frees the path (per-path
            // FSE_ALREADY_OPEN) so the MD5 pass can reopen it for read.
            nsh_close_session_file();
            nsh_map_deinit();
            nsh_parts_deinit();
        }
        g.last_tick_ms = now;
        nsh_unlock();

        // --- Slow work OUTSIDE the lock ---
        if(do_request) {
            // Small random backoff to desynchronize periodic re-request bursts.
            furi_delay_ms(furi_get_tick() % NSH_REQUEST_JITTER_MS);
            nsh_send_request(nbyte_start, nbyte_end);
            nsh_notify_led_cyan();
            // Debounce from send completion (not the decision): the long jitter above
            // must not let the next re-request fire before this REQUEST's DATA has had
            // a full round trip to come back.
            nsh_lock();
            g.last_rx_ms = g.cb_now_ms ? g.cb_now_ms() : now;
            nsh_unlock();
        }

        if(do_finalize) {
            // MD5 on a LOCAL handle; r_finalizing keeps the RX thread out of the file.
            Storage* storage = furi_record_open(RECORD_STORAGE);
            File* file = storage ? storage_file_alloc(storage) : NULL;
            unsigned char real_md5[16];
            memset(real_md5, 0, sizeof(real_md5));
            FS_Error err = 0;
            bool md5_ok = false;
            if(file) {
                md5_ok = nsh_md5_calc_file_progress(file, g.r_file_path, real_md5, &err);
                storage_file_free(file);
            }
            furi_record_close(RECORD_STORAGE);

            if(!md5_ok || err != 0) {
                FURI_LOG_E(TAG, "nsh_idle: MD5 error %d", err);
            }

            bool success = md5_ok && (err == 0) && (memcmp(real_md5, g.r_md5, 16) == 0);
            if(success) {
                FURI_LOG_I(TAG, "nsh_idle: MD5 match, file received successfully");
            } else {
                FURI_LOG_W(TAG, "nsh_idle: MD5 mismatch/error, file may be corrupted");
            }

            nsh_lock();
            g.r_is_success = success;
            g.r_is_finished = true; // reception finished flag
            nsh_unlock();

            // No notification if verification was aborted by user cancel
            if(!(furi_thread_flags_get() & NSH_WORKER_STOP_FLAG)) {
                if(success)
                    nsh_notify_success();
                else
                    nsh_notify_error();
            }
        }
        return;
    }

    g.last_tick_ms = now;
}

// ===== Packet reception and mini state machine =====

// Reject file names that could escape /ext/inbox/ (path separators / traversal)
// or are empty. `name` must already be NUL-terminated.
static bool nsh_filename_is_safe(const char* name) {
    if(name[0] == '\0') return false;
    if(strchr(name, '/') != NULL) return false;
    if(strchr(name, '\\') != NULL) return false;
    if(strstr(name, "..") != NULL) return false;
    return true;
}

static void nsh_handle_announce(uint8_t tx_id, const NSH_pl_announce_t* ann) {
    if(g.mode != NSH_MODE_RECEIVER) return;

    // notifications to fire after releasing the lock
    bool notify_vibro = false, notify_red = false, notify_blue = false;

    nsh_lock();

    // Ignore all announces while finalizing/finished — this is what prevents an
    // ANNOUNCE from re-locking and truncating the file that nsh_idle is hashing.
    if(g.r_finalizing || g.r_is_finished) {
        nsh_unlock();
        return;
    }

    // NOTE: do NOT reset last_rx_ms on an ANNOUNCE. On the receiver last_rx_ms
    // drives the re-request timeout; it must track DATA progress (and our own sent
    // REQUESTs) only. If announces reset it, a sender that keeps announcing at a
    // period <= the re-request timeout while idle at end-of-stream would forever
    // suppress the receiver's re-request -> stall that only clears when the link is
    // physically broken (which stops the announces). The initial lock seeds it below.

    if(g.r_locked == false) {
        // #8: sanitize the announced file name before using it in a path
        char name[NSH_FILENAME_LENGTH];
        memcpy(name, ann->file_name, NSH_FILENAME_LENGTH);
        name[NSH_FILENAME_LENGTH - 1] = '\0';
        if(!nsh_filename_is_safe(name)) {
            FURI_LOG_W(TAG, "nsh_handle_announce: rejected unsafe file name");
            nsh_unlock();
            nsh_notify_led_red();
            return;
        }

        FURI_LOG_I(
            TAG,
            "nsh_handle_announce: LOCK to tx_id=%d, file_name='%s', file_size=%lu",
            tx_id,
            name,
            ann->file_size);
        g.r_locked = true;
        g.r_locked_tx_id = tx_id;
        g.tx_id = tx_id; // respond on the same tx_id
        g.r_start_ms = g.cb_now_ms ? g.cb_now_ms() : 0; // reception start (for ETA / speed)
        g.r_finnsh_ms = 0;
        g.r_last_progress_ms = g.r_start_ms; // seed stall detector so we aren't "stalled" at t0
        memcpy(g.r_file_name, name, NSH_FILENAME_LENGTH);
        g.r_file_size = ann->file_size;
        memcpy(g.r_md5, ann->hash_md5, sizeof(g.r_md5));
        g.r_blocks_needed = (g.r_file_size + NSH_DATA_LENGTH - 1) / NSH_DATA_LENGTH; // round up

        // build fullpath as /ext/<dir>/<file_name>
        snprintf(
            (char*)g.r_file_path,
            sizeof(g.r_file_path),
            "/ext/%s/%s",
            NSH_RECEIVER_DIRECTORY,
            g.r_file_name);
        FURI_LOG_I(TAG, "nsh_handle_announce: r_file_path='%s'", g.r_file_path);

        nsh_ensure_inbox_dir();
        if(nsh_file_create_truncate(g.r_file_size) != 0) {
            FURI_LOG_E(TAG, "nsh_handle_announce: Failed to create/truncate file");
            g.r_locked = false; // release lock if failed to create file
            g.r_locked_tx_id = 0;
            nsh_unlock();
            return;
        }

        FURI_LOG_I(TAG, "nsh_handle_announce: Init nsh_map for %lu blocks...", g.r_blocks_needed);
        if(!nsh_map_init(g.r_blocks_needed)) {
            FURI_LOG_E(TAG, "nsh_handle_announce: Failed to init block map");
            nsh_close_session_file(); // drop the just-opened write handle
            g.r_locked = false; // release lock if failed to init map
            g.r_locked_tx_id = 0;
            nsh_unlock();
            return;
        }
        // Init parts for GUI progress bar
        FURI_LOG_I(
            TAG, "nsh_handle_announce: Init nsh_parts for %lu blocks...", g.r_blocks_needed);
        if(!nsh_parts_init(g.r_blocks_needed)) {
            FURI_LOG_E(TAG, "nsh_handle_announce: Failed to init parts");
            nsh_close_session_file(); // drop the just-opened write handle
            g.r_locked = false;
            g.r_locked_tx_id = 0;
            nsh_unlock();
            return;
        }

        // Request the first missing range immediately instead of waiting a full
        // NSH_RX_TIMEOUT_MS: seed last_rx_ms in the past so the next nsh_idle tick
        // fires the REQUEST right away (faster session start).
        g.last_rx_ms = (g.cb_now_ms ? g.cb_now_ms() : 0) - (uint32_t)NSH_RX_TIMEOUT_MS - 1u;

        notify_vibro = true;
    }

    // If already locked on another sender — ignore
    if(g.r_locked && g.r_locked_tx_id != tx_id) {
        notify_red = true;
    } else {
        notify_blue = true;
    }

    nsh_unlock();

    if(notify_vibro) nsh_notify_vibro();
    if(notify_red)
        nsh_notify_led_red();
    else if(notify_blue)
        nsh_notify_led_blue();
}

static void nsh_handle_request(uint8_t tx_id, const NSH_pl_request_t* rq) {
    if(g.mode != NSH_MODE_SENDER) return;

    FURI_LOG_D(
        TAG,
        "nsh_handle_request: tx_id=%d, range_start=%lu, range_end=%lu",
        tx_id,
        rq->range_start,
        rq->range_end);

    bool notify_cyan = false;

    nsh_lock();
    g.last_rx_ms = g.cb_now_ms ? g.cb_now_ms() : 0;

    if(tx_id != g.tx_id) {
        nsh_unlock();
        FURI_LOG_W(TAG, "nsh_handle_request: tx_id=%d != g.tx_id, ignoring", tx_id);
        nsh_notify_led_red();
        return;
    }
    // Latest REQUEST wins: even if we are mid-serving a range, adopt the new one.
    // The receiver only re-requests on its RX timeout (i.e. it is NOT receiving
    // data), so a fresh REQUEST reflects what it actually needs now — ignoring it
    // and clinging to a stale range is exactly what could wedge the transfer.
    // (Its requests exclude already-received blocks, so this does not thrash.)

    // Normalize to blocks (uneven tail == file_size allowed)
    uint32_t start = rq->range_start;
    uint32_t end = rq->range_end;
    if(end > g.s_file_size) end = g.s_file_size;
    if(start >= end) {
        nsh_unlock();
        return;
    }

    uint32_t first_block = start / NSH_DATA_LENGTH;
    uint32_t last_block = (end - 1) / NSH_DATA_LENGTH;

    g.s_is_blocks_requested = 1;
    g.s_block_needed_first = first_block;
    g.s_block_needed_last = last_block;
    g.state = NSH_ST_CONNECTED; // a receiver is talking to us -> stop announcing
    notify_cyan = true;
    nsh_unlock();

    FURI_LOG_D(
        TAG,
        "nsh_handle_request: tx_id=%d, bytes (%lu, %lu), blocks (%lu, %lu)",
        tx_id,
        start,
        end,
        first_block,
        last_block);

    if(notify_cyan) nsh_notify_led_cyan();
}

static void nsh_handle_data(uint8_t tx_id, const NSH_pl_data_t* d) {
    if(g.mode != NSH_MODE_RECEIVER) return;

    // Whole body under the lock (incl. the write): the RX thread is the only
    // writer of the map/counter, so holding the lock across my_write_block just
    // makes nsh_idle/GUI wait briefly and guarantees no interleave with the
    // finalization stage.
    nsh_lock();

    if(g.r_finalizing || g.r_is_finished) {
        nsh_unlock();
        return;
    }

    g.last_rx_ms = g.cb_now_ms ? g.cb_now_ms() : 0;

    if(!g.r_locked || tx_id != g.r_locked_tx_id) {
        nsh_unlock();
        return;
    }

    if(nsh_map_get(d->block_number) == 1) { // already received
        nsh_unlock();
        FURI_LOG_W(TAG, "nsh_handle_data: block %lu already received", d->block_number);
        return;
    }

    if(!g.cb_write_block) {
        nsh_unlock();
        return;
    }

    // Calculate valid block length based on file size
    uint32_t block_start = d->block_number * NSH_DATA_LENGTH;
    if(block_start >= g.r_file_size) {
        nsh_unlock();
        return;
    } // out of range

    uint32_t remaining = g.r_file_size - block_start;
    uint32_t valid_len = (remaining >= NSH_DATA_LENGTH) ? NSH_DATA_LENGTH : remaining;

    g.cb_write_block(d->block_number, d->data, valid_len);

    nsh_map_set(d->block_number, 1); // mark block as received
    nsh_parts_on_block_set(d->block_number); // handle parts
    g.r_blocks_received++;
    g.r_last_progress_ms = g.last_rx_ms; // real progress timestamp (for stall detection)

    uint32_t received = g.r_blocks_received;
    uint32_t needed = g.r_blocks_needed;
    nsh_unlock();

    if(d->block_number % 10 == 0) {
        nsh_notify_led_green();
    }

    FURI_LOG_I(
        TAG,
        "nsh_handle_data[txid=%d]: block %lu written, valid_len=%lu, "
        "blocks_received: %lu/%lu",
        tx_id,
        d->block_number,
        valid_len,
        received,
        needed);
}

// Main entry for a packet delivered by the NFC transport.
void nsh_receive_callback(const uint8_t* buf, size_t size) {
    uint8_t tx_id = 0, pkt_type = 0;
    const uint8_t* payload = NULL;
    if(!nsh_packet_check_and_parse(buf, size, &tx_id, &pkt_type, &payload)) {
        // Bad CRC / wrong length / not our version: drop and let ARQ retransmit.
        return;
    }

    switch((nsh_pkt_type_t)pkt_type) {
    case NSH_PKT_ANNOUNCE: {
        // LOG_D: this runs on the NfcWorker thread before the NFC reply is
        // sent; a CDC-blocked log here can push the reply past the FWT.
        FURI_LOG_D(TAG, "Received ANNOUNCE, tx_id %d", tx_id);
        NSH_pl_announce_t ann;
        nsh_pl_announce_unpack(payload, &ann);
        nsh_handle_announce(tx_id, &ann);
    } break;
    case NSH_PKT_REQUEST: {
        FURI_LOG_D(TAG, "Received REQUEST, tx_id %d", tx_id);
        NSH_pl_request_t rq;
        nsh_pl_request_unpack(payload, &rq);
        nsh_handle_request(tx_id, &rq);
    } break;
    case NSH_PKT_DATA: {
        NSH_pl_data_t d;
        nsh_pl_data_unpack(payload, &d);
        nsh_handle_data(tx_id, &d);
    } break;
    default:
        nsh_notify_led_red();
        FURI_LOG_E(TAG, "Unknown packet type: %d", pkt_type);
        break;
    }
}
