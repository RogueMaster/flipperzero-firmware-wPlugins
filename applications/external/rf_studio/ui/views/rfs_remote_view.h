#pragma once

#include <gui/view.h>
#include "../../remote/rfs_remote.h"
#include "../../signal/rfs_signal.h"

/*
 * Remote operation screen (Addendum 001 Section 3).
 * Up/Down select, Left/Right page, OK transmits, hold OK opens the remote menu.
 */

typedef enum {
    RfsRemoteViewEventSend,
    RfsRemoteViewEventMenu,
} RfsRemoteViewEvent;

typedef enum {
    RfsStatusNone,
    RfsStatusBusy,
    RfsStatusOk,
    RfsStatusError,
} RfsStatusKind;

typedef void (*RfsRemoteViewCallback)(RfsRemoteViewEvent event, void* context);

typedef struct RfsRemoteView RfsRemoteView;

RfsRemoteView* rfs_remote_view_alloc(void);
void rfs_remote_view_free(RfsRemoteView* view);
View* rfs_remote_view_get_view(RfsRemoteView* view);

void rfs_remote_view_set_callback(
    RfsRemoteView* view,
    RfsRemoteViewCallback callback,
    void* context);

/** Show a remote. The remote and index must stay valid while the view is displayed. */
void rfs_remote_view_set_remote(
    RfsRemoteView* view,
    const RfsRemote* remote,
    const RfsSignalIndex* index,
    int32_t selected);

int32_t rfs_remote_view_get_selected(RfsRemoteView* view);
void rfs_remote_view_set_status(RfsRemoteView* view, const char* text, RfsStatusKind kind);
