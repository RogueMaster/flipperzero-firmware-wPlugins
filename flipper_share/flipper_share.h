#include <stdint.h>
#include <stdlib.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#define FS_PACKET_LENGTH 60                                 // Total max packet length
#define FS_PAYLOAD_LENGTH (FS_PACKET_LENGTH - 4)            // 4 for version, tx_id, packet_type, crc
#define FS_FILENAME_LENGTH (FS_PAYLOAD_LENGTH - 4 - 16)     // 4 for file size, 16 for MD5 hash
#define FS_DATA_LENGTH (FS_PAYLOAD_LENGTH - 4)              // 4 for block number

#define FS_TIMEOUT_IDLE 1000                                // ms
#define FS_TIMEOUT_BETWEEN_PACKETS 120                      // 70 ms        // TODO: optimize?

#define FS_SUBGHZ_RETRY_DELAY_MS 1000                       // For Retry On error

#define FS_PARTS_COUNT 100u                                 // For progress bar in GUI

#define FS_PARTS_BYTES ((uint32_t)((FS_PARTS_COUNT + 7u) / 8u))

// Packet structure:
typedef struct {
    uint8_t version;
    uint8_t tx_id;
    uint8_t packet_type;
    uint8_t payload[FS_PAYLOAD_LENGTH];
    uint8_t crc;
} FS_packet_t;

// Packet types:
typedef enum {
    FS_PKT_ANNOUNCE = 1,
    FS_PKT_REQUEST  = 2,
    FS_PKT_DATA     = 3,
} fs_pkt_type_t;

// Payloads:
typedef struct {
    uint8_t file_name[FS_FILENAME_LENGTH];
    uint32_t file_size;
    uint8_t hash_md5[16];
} FS_pl_announce_t;

typedef struct {
    uint32_t range_start;           // Bytes, not blocks
    uint32_t range_end;             // Bytes, not blocks
} FS_pl_request_t;

typedef struct {
    uint32_t block_number;          // Block number, not byte offset
    uint8_t data[FS_DATA_LENGTH];
} FS_pl_data_t;

// Modes:
typedef enum {
    FS_MODE_NONE = 0,
    FS_MODE_SENDER,
    FS_MODE_RECEIVER
} fs_mode_t;

typedef enum {
    FS_ST_IDLE = 0,
    FS_ST_ANNOUNCING,     // sender
    FS_ST_RECEIVING       // receiver
} fs_state_t;

typedef struct {
    fs_mode_t  mode;
    fs_state_t state;
    uint8_t    tx_id;                   // random pseudo-session tx_id, must match on both sides
    uint32_t   last_tick_ms;            // for internal step
    uint32_t   last_announce_ms;        //
    uint32_t   last_rx_ms;              // LRU rx

    void (*cb_send_bytes)(const uint8_t* buf, size_t len);          // Radio send bytes callback
    uint32_t (*cb_now_ms)(void);                                    // Time callback, monotonic ms

    // Sender data:
    char       s_file_path[256];
    char       s_file_name[FS_FILENAME_LENGTH];
    uint32_t   s_file_size;
    unsigned char    s_md5[16];

    Storage*   storage; // Storage record for file operations
    File*      file; // File handle for reading the file
    
    uint8_t    s_is_blocks_requested;
    uint32_t   s_block_needed_first;
    uint32_t   s_block_needed_last;

    // Callback for reading a data block (FS_DATA_LENGTH bytes)
    // MUST fill out52 (zero-pad the incomplete last block).
    // Returns the actual number of "valid" bytes in this block (for the last one),
    // usually 52, and for the last one — the remainder. (The receiver will trim it based on file_size)
    uint32_t (*cb_read_block)(uint32_t block_number, uint8_t out52[FS_DATA_LENGTH]);        // Reader callback from real storage

    // Receiver data:
    bool       r_locked;                                // if tx_id of receiver matches to tx_id of sender
    uint8_t    r_locked_tx_id;                          // TODO: no need for receiver?

    char       r_file_path[256];                        // Build actual path when announce handling and lock session to tx_id
    char       r_file_name[FS_FILENAME_LENGTH];
    uint32_t   r_file_size;
    uint32_t   r_blocks_needed; // blocks total needed, calculated from file_size during announce handling
    uint32_t   r_blocks_received; // how many have been received
    unsigned char    r_md5[16];
    bool       r_is_finished;
    bool       r_is_success;

    // Callback for writing a received block data by number to real storage.
    // in52 is always 52 bytes, but must write min(52, remainder).
    void (*cb_write_block)(uint32_t block_number, const uint8_t in52[FS_DATA_LENGTH], uint32_t valid_len);
} fs_ctx_t;

extern fs_ctx_t g;      // extern to be available in GUI

// Public API:

// Init structure for both: Receiver and Sender
typedef struct {
    fs_mode_t mode;                        // SENDER / RECEIVER
    uint8_t   tx_id;                       // pseudo-random session ID for SENDER, RECEIVER will lock to it on ANNOUNCE handling

    void     (*send_bytes)(const uint8_t* buf, size_t len);   // Radio send bytes callback
    uint32_t (*now_ms)(void);                                 // Time callback, monotonic ms

    // sender-specific, (required if mode == SENDER)
    char  s_file_path[256];
    uint32_t     (*s_read_block)(uint32_t block_number, uint8_t out52[FS_DATA_LENGTH]);

    // receiver-specific (required if mode == RECEIVER)
    void (*r_write_block)(uint32_t block_number, const uint8_t in52[FS_DATA_LENGTH], uint32_t valid_len);
} fs_init_params_t;

// API functions:
bool fs_init_from_external_transmit();
bool fs_init_from_external_receive();
bool fs_init(const fs_init_params_t* p);
void fs_deinit(void);
void fs_idle(void); // to be called periodically from main loop (50ms?)

// High-level sending (can be called from outside):
void fs_send_announce(void);
void fs_send_request(uint32_t range_start, uint32_t range_end);
// void fs_send_data(uint32_t block_number);
void fs_send_data(void);

// External callback for receiving "raw" 60 bytes from radio:
void fs_receive_callback(const uint8_t* buf, size_t size);

// Parts for progress bar in GUI

bool     fs_parts_init(uint32_t block_count);
void     fs_parts_reset(void);
void     fs_parts_on_block_set(uint32_t block_index);
int      fs_parts_get(uint32_t part_index);
void     fs_parts_bitmap_copy(uint8_t* dst);

uint32_t fs_parts_count(void);
uint32_t fs_parts_block_count(void);
bool     fs_parts_is_ready(void);