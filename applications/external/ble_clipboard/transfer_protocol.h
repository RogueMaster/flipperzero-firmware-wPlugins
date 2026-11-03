#pragma once
#include "macro_engine.h"
#include "saved_program.h"

#define CLIPBOARD_WIRE_VERSION 4
#define CLIPBOARD_STANDALONE   1U
#define CLIPBOARD_SAVE_ONLY    2U
#define CLIPBOARD_CHUNK_MAX    512
#define CLIPBOARD_FRAME_MAX    (CLIPBOARD_CHUNK_MAX + 12)

typedef enum {
    ClipboardHello = 1,
    ClipboardBegin,
    ClipboardChunk,
    ClipboardCommit,
    ClipboardExecute,
    ClipboardCancel,
    ClipboardSave,
    ClipboardInfo,
    ClipboardReady = 0x81,
    ClipboardBegun,
    ClipboardChunked,
    ClipboardVerified,
    ClipboardStarted,
    ClipboardWaitingUSB,
    ClipboardProgress,
    ClipboardDone,
    ClipboardSavedReply,
    ClipboardActive,
    ClipboardInfoReply = 0x8B,
    ClipboardError = 0xFF,
} ClipboardCommand;

typedef enum {
    ClipboardJobIdle,
    ClipboardJobReceiving,
    ClipboardJobVerified,
    ClipboardJobWaitingUSB,
    ClipboardJobRunning,
} ClipboardJobState;

typedef struct {
    void* context;
    bool (*send)(void* context, const uint8_t* data, size_t size);
    bool (*open)(void* context, size_t size);
    bool (*write)(void* context, size_t offset, const uint8_t* data, size_t size);
    bool (*read)(void* context, size_t offset, uint8_t* output, size_t size);
    void (*close)(void* context);
    bool (*load_layout)(void* context, uint8_t layout, uint8_t target, uint16_t output[128]);
    bool (*usb_connected)(void* context);
    bool (*press)(void* context, uint16_t key);
    bool (*release)(void* context);
    bool (*mouse_move)(void* context, int8_t dx, int8_t dy);
    bool (*save_program)(void* context, const ClipboardSaved* saved);
    bool (*random)(void* context, uint32_t* value);
} ClipboardServerIO;

typedef struct {
    ClipboardServerIO io;
    ClipboardJobState state;
    ClipboardEngine engine;
    bool authenticated;
    bool link_connected;
    uint8_t flags;
    uint32_t started_at;
    uint32_t execution_cursor;
    uint8_t frame[CLIPBOARD_FRAME_MAX];
    size_t frame_size;
    size_t frame_expected;
    uint32_t job_id;
    uint32_t total;
    uint32_t received;
    uint32_t expected_crc;
    uint32_t crc;
    uint32_t activity_at;
    uint32_t progress_at;
    uint8_t layout_id;
    uint8_t target;
    uint16_t layout[128];
    char device_name[32];
} ClipboardServer;

uint32_t clipboard_crc_update(uint32_t crc, const uint8_t* data, size_t size);
void clipboard_server_init(ClipboardServer* server, ClipboardServerIO io, const char* device_name);
void clipboard_server_feed(ClipboardServer* server, const uint8_t* data, size_t size, uint32_t now);
void clipboard_server_tick(ClipboardServer* server, uint32_t now, bool connected);
void clipboard_server_disconnect(ClipboardServer* server);
void clipboard_server_cancel(ClipboardServer* server);
bool clipboard_server_run_saved(
    ClipboardServer* server,
    const ClipboardSaved* saved,
    uint32_t id,
    uint32_t now);
