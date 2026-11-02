#pragma once

#include <stdbool.h>
#include <stddef.h>

/* KeePass CMD wrapper, restricted to a single deferred native launcher request.
 * Placeholder expansion precedes final destination/argument validation. */
#define FLIPPASS_CMD_MAX_WRAPPER_BYTES     4095U
#define FLIPPASS_CMD_MAX_APPLICATION_BYTES 1023U
#define FLIPPASS_CMD_SCRATCH_EXTRA         16U
#define FLIPPASS_CMD_MAX_EXPANSION_DEPTH   12U

typedef enum {
    FlipPassCmdNotCommand = 0,
    FlipPassCmdValid,
    FlipPassCmdInvalid,
} FlipPassCmdParseResult;

typedef struct {
    const char* text;
    size_t length;
    size_t consumed;
} FlipPassCmd;

typedef struct {
    const char* application;
    const char* arguments;
    bool self;
} FlipPassCmdLaunch;

FlipPassCmdParseResult flippass_cmd_parse(const char* sequence, FlipPassCmd* command);
bool flippass_cmd_decode(
    const FlipPassCmd* command,
    char* scratch,
    size_t capacity,
    FlipPassCmdLaunch* launch);

/* Resolver returns a borrowed text span. NULL output measures the exact bounded
 * result. Field values recurse, quoted replacements escape native argv bytes;
 * active commands/keys are never executed by this compiler. */
typedef bool (*FlipPassCmdResolve)(
    void* context,
    const char* name,
    size_t name_size,
    const char** value,
    size_t* value_size);
bool flippass_cmd_expand(
    const FlipPassCmd* command,
    FlipPassCmdResolve resolve,
    void* context,
    char* output,
    size_t capacity,
    size_t* bytes);

/* Refuses multiple commands, malformed wrappers and CMD mixed with OTP before
 * any HID output. The ordinary AutoType parser handles non-CMD tokens. */
bool flippass_cmd_preflight(const char* sequence);
