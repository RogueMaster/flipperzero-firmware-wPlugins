#pragma once

#include "../xremote.h"
//#include <gui/view.h>
#include "../helpers/xremote_custom_event.h"

// Pause duration is stored in SECONDS. Displayed as MM:SS.
#define PAUSE_TIME_LENGTH           6 // "MM:SS\0"
#define PAUSE_TIME_FORMAT           "%02d:%02d"
#define XREMOTE_PAUSE_MAX_SECONDS   3600 // 60 minutes
#define XREMOTE_PAUSE_FAST_STEP     5 // seconds added per held (repeat) press
#define XREMOTE_PAUSE_FIELD_MINUTES 0
#define XREMOTE_PAUSE_FIELD_SECONDS 1

typedef struct XRemotePauseSet XRemotePauseSet;

typedef void (*XRemotePauseSetCallback)(XRemoteCustomEvent event, void* context);

void xremote_pause_set_set_callback(
    XRemotePauseSet* instance,
    XRemotePauseSetCallback callback,
    void* context);

XRemotePauseSet* xremote_pause_set_alloc();
void xremote_pause_set_free(XRemotePauseSet* instance);

void xremote_pause_set_enter(void* context);

View* xremote_pause_set_get_view(XRemotePauseSet* instance);
