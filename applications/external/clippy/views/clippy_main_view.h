#pragma once

#include <gui/view.h>

typedef struct ClippyMain ClippyMain;
typedef void (*ClippyMainButtonCallback)(InputKey key, void* context);

ClippyMain* clippy_main_view_alloc(void);

void clippy_main_view_free(ClippyMain* main);

View* clippy_main_view_get_view(ClippyMain* main);

void clippy_main_view_set_button_callback(
    ClippyMain* main,
    ClippyMainButtonCallback callback,
    void* context);
