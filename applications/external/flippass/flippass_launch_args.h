#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Limits exclude the terminating NUL and count encoded bytes, not characters. */
#define FLIPPASS_LAUNCH_ARGS_MAX_BYTES     4095U
#define FLIPPASS_LAUNCH_PATH_MAX_BYTES     1023U
#define FLIPPASS_LAUNCH_PASSWORD_MAX_BYTES 255U
#define FLIPPASS_LAUNCH_FIELD_MAX_BYTES    127U
#define FLIPPASS_LAUNCH_LAYOUT_MAX_BYTES   63U
#define FLIPPASS_LAUNCH_TOKEN_COUNT        7U

typedef enum {
    FlipPassLaunchTransportAuto = 0,
    FlipPassLaunchTransportUsb,
    FlipPassLaunchTransportBluetooth,
} FlipPassLaunchTransport;

typedef enum {
    FlipPassLaunchArgsOk = 0,
    FlipPassLaunchArgsInvalidArgument,
    FlipPassLaunchArgsTooLong,
    FlipPassLaunchArgsScratchTooSmall,
    FlipPassLaunchArgsTooManyTokens,
    FlipPassLaunchArgsInvalidQuotes,
    FlipPassLaunchArgsInvalidEscape,
    FlipPassLaunchArgsControlCharacter,
    FlipPassLaunchArgsInvalidDatabasePath,
    FlipPassLaunchArgsPathTooLong,
    FlipPassLaunchArgsPasswordTooLong,
    FlipPassLaunchArgsFieldTooLong,
    FlipPassLaunchArgsFieldWithoutEntry,
    FlipPassLaunchArgsInvalidTransport,
    FlipPassLaunchArgsLayoutTooLong,
    FlipPassLaunchArgsInvalidLayout,
    FlipPassLaunchArgsInvalidAutoclose,
} FlipPassLaunchArgsError;

typedef struct {
    const char* database_path;
    const char* password;
    const char* entry_path;
    const char* field_tag;
    const char* layout_id;
    FlipPassLaunchTransport transport;
    bool autoclose;
    uint8_t token_count;
    uint8_t present_mask; /* Bit zero is database path (external ordinal two). */
} FlipPassLaunchArgs;

/* Parse [database], [password], [entry], [field], [transport], [layout], [autoclose].
 * The launcher already consumed the FAP path; no command token is present. A token is
 * unquoted or wholly double-quoted. Only quote and backslash are escaped inside
 * quotes; other backslashes are literal. ASCII controls and DEL are rejected.
 * Missing database means normal startup; missing password means interactive
 * authentication. An explicit empty password is distinct from a missing one.
 * No shell expansion occurs. Empty optional transport/layout/close use defaults.
 * An old six-token invocation ending in 0 or 1 retains its autoclose meaning.
 *
 * scratch_capacity must cover the original input plus NUL. On success all string
 * pointers reference scratch, and omitted optional strings are NULL. The input
 * may alias scratch. On an error after copying, the copied scratch bytes are
 * zeroed and out stays empty. The original input/launcher copies are not erased.
 */
FlipPassLaunchArgsError flippass_launch_args_parse(
    const char* input,
    char* scratch,
    size_t scratch_capacity,
    FlipPassLaunchArgs* out);

/* Fixed error descriptions never include an argument or password. */
const char* flippass_launch_args_error_message(FlipPassLaunchArgsError error);

/* Scrub the caller-owned scratch allocation and invalidate its parsed views. */
void flippass_launch_args_clear(FlipPassLaunchArgs* args, char* scratch, size_t scratch_capacity);

/* An ASCII basename or reserved Default/Alt+NumPad ID, never a filesystem path. */
bool flippass_launch_layout_id_valid(const char* id);

#ifdef __cplusplus
}
#endif
