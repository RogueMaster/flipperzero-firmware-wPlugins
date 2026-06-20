#pragma once

#include <gui/view.h>
#include "../led_remote_types.h"

typedef struct LedRemoteViewRemote LedRemoteViewRemote;
typedef void (*LedRemoteViewRemoteCallback)(LedButton button, void* context);

LedRemoteViewRemote* led_remote_view_remote_alloc(void);
void                 led_remote_view_remote_free(LedRemoteViewRemote* view);
View*                led_remote_view_remote_get_view(LedRemoteViewRemote* view);

void led_remote_view_remote_set_callback(
    LedRemoteViewRemote* view,
    LedRemoteViewRemoteCallback callback,
    void* context);

void led_remote_view_remote_set_learned(LedRemoteViewRemote* view, const bool* learned);
void led_remote_view_remote_set_profile_name(LedRemoteViewRemote* view, const char* name);
void led_remote_view_remote_set_layout(LedRemoteViewRemote* view, LedRemoteLayout layout);

void led_remote_view_remote_set_remote_params(
    LedRemoteViewRemote* view,
    uint8_t button_count,
    uint8_t grid_cols,
    const char* const* button_labels_short);
