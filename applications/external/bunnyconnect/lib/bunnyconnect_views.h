#pragma once

#include <gui/view.h>
#include <gui/gui.h>
#include <input/input.h>

typedef struct BunnyConnectCustomView BunnyConnectCustomView;

typedef void (*BunnyConnectViewCallback)(void* context);

BunnyConnectCustomView* bunnyconnect_view_alloc(void);
void bunnyconnect_view_free(BunnyConnectCustomView* view);
View* bunnyconnect_view_get_view(BunnyConnectCustomView* view);
void bunnyconnect_view_set_callback(
    BunnyConnectCustomView* view,
    BunnyConnectViewCallback callback,
    void* context);
