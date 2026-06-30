#pragma once
#include <flip_wifi.h>

bool alloc_playlist(void *context);
bool alloc_submenus(void *context, uint32_t view);
bool alloc_text_box(FlipWiFiApp *app);
bool alloc_text_inputs(void *context, uint32_t view);
bool alloc_views(void *context, uint32_t view);
bool alloc_widgets(void *context, uint32_t widget);