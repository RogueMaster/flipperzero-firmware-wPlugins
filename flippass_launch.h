#pragma once
#include "flippass_launch_args.h"

struct App;
typedef struct FlipPassLaunchRequest {
    size_t allocation_bytes;
    const char* entry_path;
    const char* field_tag;
    const char* keyboard_layout_path; /* NULL = Default, empty = Alt+NumPad. */
    FlipPassLaunchTransport transport;
    bool autoclose;
    char data[];
} FlipPassLaunchRequest;

bool flippass_launch_start(struct App* app, const char* arguments);
void flippass_launch_tick(struct App* app);
void flippass_launch_cancel(struct App* app);
