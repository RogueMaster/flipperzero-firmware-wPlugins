#pragma once

#include <gui/view.h>
#include <notification/notification.h>

#include "bm_settings.h"

typedef struct BmMouse BmMouse;

typedef void (*BmMouseExitCallback)(void* context);

BmMouse* bm_mouse_alloc(const BmSettings* settings, NotificationApp* notifications);

void bm_mouse_free(BmMouse* bm_mouse);

View* bm_mouse_get_view(BmMouse* bm_mouse);

void bm_mouse_set_exit_callback(BmMouse* bm_mouse, BmMouseExitCallback callback, void* context);
