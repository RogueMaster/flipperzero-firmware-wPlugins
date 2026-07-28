#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    ShockerModelCaiXianlin,
    ShockerModelPetrainer,
    ShockerModelPetrainer998DR,
    ShockerModelT330,
    ShockerModelD80,
    ShockerModelCount,
} ShockerModel;

typedef enum {
    ShockerCmdShock,
    ShockerCmdVibrate,
    ShockerCmdSound,
    ShockerCmdLight,
    ShockerCmdCount,
} ShockerCommand;

// An OOK pulse: carrier on for high_us microseconds, then off for low_us microseconds.
typedef struct {
    uint16_t high_us;
    uint16_t low_us;
} OokPulse;

// Maximum pulse buffer size (dual TX concatenates two packets + gap; CaiXianlin is 44 each)
#define OPENSHOCK_MAX_PULSES 160

// Decoded shocker command (result of RX decoding).
typedef struct {
    ShockerModel model;
    uint16_t shocker_id;
    uint8_t channel;
    ShockerCommand command;
    uint8_t intensity;
} DecodedShocker;

// Returns the display name for a shocker model.
const char* openshock_model_name(ShockerModel model);

// Returns the display name for a command type.
const char* openshock_command_name(ShockerCommand command);

// Returns whether a command is supported by the given model.
bool openshock_command_supported(ShockerModel model, ShockerCommand command);

// Try to decode OOK pulses into a shocker command.
// Tries all protocols with a sliding window. Returns true on first match.
bool openshock_decode(const OokPulse* pulses, size_t count, DecodedShocker* result);

// Encode a command into OOK pulse timings.
// Returns number of pulses written, or 0 on error.
size_t openshock_encode(
    ShockerModel model,
    uint16_t shocker_id,
    ShockerCommand command,
    uint8_t intensity,
    uint8_t channel,
    OokPulse* buffer,
    size_t buffer_size);
