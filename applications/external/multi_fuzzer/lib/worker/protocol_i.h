#pragma once

#include "protocol.h"

// Timing values are expressed in 10ms units (1 = 0.01s).
// When the user selects a 0 idle/emu time, the worker falls back to this real,
// non-zero delay (ms) so the FuriTimer never gets a 0-tick interval and the
// hardware worker is never restarted with a literally zero gap.
#define FUZZ_WORKER_MIN_DELAY_MS (1)

#if defined(RFID_125_PROTOCOL)
#define MAX_PAYLOAD_SIZE (8)
#define PROTOCOL_DEF_IDLE_TIME (40)
#define PROTOCOL_DEF_EMU_TIME (50)
#define PROTOCOL_TIME_DELAY_MIN PROTOCOL_DEF_IDLE_TIME + PROTOCOL_DEF_EMU_TIME

// RFID emulation is driven directly via furi_hal_rfid (see hardware_worker.c),
// bypassing lfrfid_worker and its ~0.10s restart floor. Stop is synchronous, so
// the idle delay can go all the way to 0.00s; the worker still clamps a literal
// 0 to a tiny non-zero real delay.
#define PROTOCOL_MIN_IDLE_TIME (0)
// Emulation time is user-tunable down to 0.00s as well (worker clamps a literal
// 0 to the same tiny non-zero real delay). Lower = field on more briefly per UID.
#define PROTOCOL_MIN_EMU_TIME (0)

#define PROTOCOL_KEY_FOLDER_NAME "lfrfid"
#define PROTOCOL_KEY_EXTENSION ".rfid"

#else

#define MAX_PAYLOAD_SIZE (8)
#define PROTOCOL_DEF_IDLE_TIME (20)
#define PROTOCOL_DEF_EMU_TIME (20)
#define PROTOCOL_TIME_DELAY_MIN PROTOCOL_DEF_IDLE_TIME + PROTOCOL_DEF_EMU_TIME

// iButton serialises stop/emulate through a message queue (no idle furi_check),
// so a fast restart can't crash it. Allow the idle delay all the way to 0.00s;
// the worker still clamps a literal 0 to a tiny non-zero real delay.
#define PROTOCOL_MIN_IDLE_TIME (0)
// Emulation time is user-tunable down to 0.00s as well (worker clamps a literal
// 0 to the same tiny non-zero real delay). Lower = field on more briefly per UID.
#define PROTOCOL_MIN_EMU_TIME (0)

#define PROTOCOL_KEY_FOLDER_NAME "ibutton"
#define PROTOCOL_KEY_EXTENSION ".ibtn"
#endif

typedef struct ProtoDict ProtoDict;
typedef struct FuzzerProtocol FuzzerProtocol;

struct ProtoDict {
    const uint8_t* val;
    const uint8_t len;
};

struct FuzzerProtocol {
    const char* name;
    const uint8_t data_size;
    const ProtoDict dict;
};

// #define MAX_PAYLOAD_SIZE 6

// #define FUZZ_TIME_DELAY_MIN (5)
// #define FUZZ_TIME_DELAY_DEFAULT (10)
// #define FUZZ_TIME_DELAY_MAX (70)

// #define MAX_PAYLOAD_SIZE 8

// #define FUZZ_TIME_DELAY_MIN (4)
// #define FUZZ_TIME_DELAY_DEFAULT (8)
// #define FUZZ_TIME_DELAY_MAX (80)

extern const FuzzerProtocol fuzzer_proto_items[];
