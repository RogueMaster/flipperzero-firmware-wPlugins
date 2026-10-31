#pragma once
#include <stddef.h>

typedef struct Storage Storage;

typedef enum {
    FlipPassLaunchLayoutDefault,
    FlipPassLaunchLayoutOverride,
    FlipPassLaunchLayoutInvalidId,
    FlipPassLaunchLayoutNotFound,
    FlipPassLaunchLayoutInvalidFile,
    FlipPassLaunchLayoutUnavailable,
} FlipPassLaunchLayoutResult;

/* Default borrows the current preference. Override returns a bounded path, or
 * an empty path for Alt+NumPad. /ext is FAT: ASCII names are case-insensitive. */
FlipPassLaunchLayoutResult
    flippass_launch_layout_resolve(Storage* storage, const char* id, char* path, size_t capacity);

const char* flippass_launch_layout_error(FlipPassLaunchLayoutResult result);
