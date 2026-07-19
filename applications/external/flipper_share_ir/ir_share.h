#include <stdint.h>
#include <stdlib.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

// ===== Packet sizing =========================================================
// Wire format per packet: [version(1)][tx_id(1)][packet_type(1)][payload][crc16(2)]
#define ISH_HEADER_LENGTH   3u // version + tx_id + packet_type
#define ISH_CRC_LENGTH      2u // CRC-16 trailer
#define ISH_OVERHEAD_LENGTH (ISH_HEADER_LENGTH + ISH_CRC_LENGTH)

// Control packets (ANNOUNCE / REQUEST) must carry file_name + file_size + md5.
#define ISH_FILENAME_LENGTH     36u // max file name incl. NUL
#define ISH_CTRL_PAYLOAD_LENGTH (ISH_FILENAME_LENGTH + 4u + 16u) // name + size + md5
#define ISH_CTRL_PACKET_LENGTH  (ISH_CTRL_PAYLOAD_LENGTH + ISH_OVERHEAD_LENGTH)

// DATA packets: block_number(4) + file data. ISH_DATA_LENGTH is THE tunable knob
// (file-data bytes per packet); it is decoupled from the control-packet size so
// it can be shrunk for noisier IR links without limiting the file name length.
// Recompile to change. Keep ISH_DATA_PACKET_LENGTH <= ISH_MODEM max frame bytes.
#ifndef ISH_DATA_LENGTH
#define ISH_DATA_LENGTH 52u
#endif
#define ISH_DATA_PAYLOAD_LENGTH (ISH_DATA_LENGTH + 4u) // block_number + data
#define ISH_DATA_PACKET_LENGTH  (ISH_DATA_PAYLOAD_LENGTH + ISH_OVERHEAD_LENGTH)

// Buffers are sized to the larger of the two packet kinds.
#define ISH_PAYLOAD_MAX                                                              \
    ((ISH_CTRL_PAYLOAD_LENGTH > ISH_DATA_PAYLOAD_LENGTH) ? ISH_CTRL_PAYLOAD_LENGTH : \
                                                           ISH_DATA_PAYLOAD_LENGTH)
#define ISH_PACKET_MAX (ISH_PAYLOAD_MAX + ISH_OVERHEAD_LENGTH)

// ===== Behaviour tunables ====================================================
#define ISH_PAYLOAD_THROUGHPUT_BPS \
    130 // nominal IR payload throughput for ETA, bytes/sec (measured ~131 on 8 KiB after Tier-1 tuning)
#define ISH_ETA_WARMUP_MS 10000 // below this elapsed, ETA uses the constant fallback
#define ISH_STALL_MS      12000 // no new block for this long -> show "stalled"
#define ISH_ETA_MAX_SEC \
    (99u * 3600u + 59u * 60u + 59u) // clamp ETA display (avoid uint32 overflow / garbage)

#define ISH_HASH_CHUNK_SIZE  4096 // bytes per MD5 chunk (hash progress granularity)
#define ISH_WORKER_STOP_FLAG 0x1u // FuriThread flag: stop worker (also aborts in-progress hashing)

#define ISH_PARTS_COUNT 100u // For progress bar in GUI
#define ISH_PARTS_BYTES ((uint32_t)((ISH_PARTS_COUNT + 7u) / 8u))

// Packet types:
typedef enum {
    ISH_PKT_ANNOUNCE = 1,
    ISH_PKT_REQUEST = 2,
    ISH_PKT_DATA = 3,
} ish_pkt_type_t;

// Payloads:
typedef struct {
    uint8_t file_name[ISH_FILENAME_LENGTH];
    uint32_t file_size;
    uint8_t hash_md5[16];
} ISH_pl_announce_t;

typedef struct {
    uint32_t range_start; // Bytes, not blocks
    uint32_t range_end; // Bytes, not blocks
} ISH_pl_request_t;

typedef struct {
    uint32_t block_number; // Block number, not byte offset
    uint8_t data[ISH_DATA_LENGTH];
} ISH_pl_data_t;

// Modes:
typedef enum {
    ISH_MODE_NONE = 0,
    ISH_MODE_SENDER,
    ISH_MODE_RECEIVER
} ish_mode_t;

typedef enum {
    ISH_ST_IDLE = 0,
    ISH_ST_ANNOUNCING, // sender: broadcasting metadata, waiting for a receiver
    ISH_ST_CONNECTED, // sender: a receiver locked on -> stop announcing, serve requests
    ISH_ST_RECEIVING // receiver
} ish_state_t;

typedef struct {
    ish_mode_t mode;
    ish_state_t state;
    uint8_t tx_id; // random pseudo-session tx_id, must match on both sides
    uint32_t last_tick_ms; // for internal step
    uint32_t last_announce_ms; //
    uint32_t last_rx_ms; // LRU rx

    void (*cb_send_bytes)(const uint8_t* buf, size_t len); // Radio send bytes callback
    uint32_t (*cb_now_ms)(void); // Time callback, monotonic ms

    // Sender data:
    char s_file_path[256];
    char s_file_name[ISH_FILENAME_LENGTH];
    uint32_t s_file_size;
    unsigned char s_md5[16];

    Storage* storage; // Storage record for file operations
    File* file; // File handle for reading the file

    uint8_t s_is_blocks_requested;
    uint32_t s_block_needed_first;
    uint32_t s_block_needed_last;

    // Callback for reading a data block (ISH_DATA_LENGTH bytes)
    // MUST fill out52 (zero-pad the incomplete last block).
    // Returns the actual number of "valid" bytes in this block (for the last one),
    // usually 52, and for the last one — the remainder. (The receiver will trim it based on file_size)
    uint32_t (*cb_read_block)(
        uint32_t block_number,
        uint8_t out52[ISH_DATA_LENGTH]); // Reader callback from real storage

    // Receiver data:
    bool r_locked; // if tx_id of receiver matches to tx_id of sender
    uint8_t r_locked_tx_id; // TODO: no need for receiver?

    char r_file_path[256]; // Build actual path when announce handling and lock session to tx_id
    char r_file_name[ISH_FILENAME_LENGTH];
    uint32_t r_file_size;
    uint32_t
        r_blocks_needed; // blocks total needed, calculated from file_size during announce handling
    uint32_t r_blocks_received; // how many have been received
    unsigned char r_md5[16];
    bool r_is_finished;
    bool r_is_success;
    bool r_finalizing; // set while computing final MD5; receiver ignores incoming packets
    uint32_t r_start_ms; // set on ANNOUNCE lock — reception start time
    uint32_t r_finish_ms; // set when all blocks received (finalization) — reception end time
    uint32_t r_last_progress_ms; // time of last accepted (new) block — for stall detection

    // Callback for writing a received block data by number to real storage.
    // in52 is always 52 bytes, but must write min(52, remainder).
    void (*cb_write_block)(
        uint32_t block_number,
        const uint8_t in52[ISH_DATA_LENGTH],
        uint32_t valid_len);
} ish_ctx_t;

extern ish_ctx_t g; // extern to be available in GUI

// Public API:

// Init structure for both: Receiver and Sender
typedef struct {
    ish_mode_t mode; // SENDER / RECEIVER
    uint8_t
        tx_id; // pseudo-random session ID for SENDER, RECEIVER will lock to it on ANNOUNCE handling

    void (*send_bytes)(const uint8_t* buf, size_t len); // Radio send bytes callback
    uint32_t (*now_ms)(void); // Time callback, monotonic ms

    // sender-specific, (required if mode == SENDER)
    char s_file_path[256];
    uint32_t (*s_read_block)(uint32_t block_number, uint8_t out52[ISH_DATA_LENGTH]);

    // receiver-specific (required if mode == RECEIVER)
    void (*r_write_block)(
        uint32_t block_number,
        const uint8_t in52[ISH_DATA_LENGTH],
        uint32_t valid_len);
} ish_init_params_t;

// API functions:
bool ish_init_from_external_transmit();
bool ish_init_from_external_receive();
bool ish_init(const ish_init_params_t* p);
void ish_deinit(void);
void ish_idle(void); // to be called periodically from main loop (50ms?)

// Shared-state lock (protects `g`, g_map and ish_parts across the worker,
// SubGhz RX-callback and GUI threads). Create it in the scene on_enter BEFORE
// starting any worker/radio thread; ish_deinit() frees it.
void ish_lock_ensure(void); // idempotent, alloc mutex if missing
void ish_lock(void); // blocking acquire (no-op if not created)
void ish_unlock(void); // release (no-op if not created)
bool ish_try_lock_ms(uint32_t timeout_ms); // timed acquire, true if acquired (for GUI callbacks)

// High-level sending (can be called from outside):
void ish_send_announce(void);
void ish_send_request(uint32_t range_start, uint32_t range_end);
// void ish_send_data(uint32_t block_number);
void ish_send_data(void);

// External callback: called by the IR transport with one decoded packet's bytes.
void ish_receive_callback(const uint8_t* buf, size_t size);

// Format a duration adaptively: "M:SS" under 1h, "H:MM:SS" from 1h (for GUI).
void ish_fmt_duration(uint32_t secs, char* buf, size_t n);

// Snapshot of the in-progress chunked MD5 (bytes done / total) for the GUI.
// Returns false if the shared-state lock is contended — skip the tick then.
bool ish_hash_progress_get(uint32_t* done, uint32_t* total);

// Parts for progress bar in GUI

bool ish_parts_init(uint32_t block_count);
void ish_parts_reset(void);
void ish_parts_on_block_set(uint32_t block_index);
int ish_parts_get(uint32_t part_index);
void ish_parts_levels_copy(uint8_t* dst); // dst[ISH_PARTS_COUNT], per-part fill level 0..255

uint32_t ish_parts_count(void);
uint32_t ish_parts_block_count(void);
bool ish_parts_is_ready(void);
