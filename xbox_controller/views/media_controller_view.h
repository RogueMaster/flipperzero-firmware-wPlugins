#pragma once

#include <gui/view.h>
#include <furi.h>
#include <gui/elements.h>
#include <xc_icons.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef struct MediaControllerView MediaControllerView;

MediaControllerView* media_controller_view_alloc();

void media_controller_view_free(MediaControllerView* media_controller_view);

View* media_controller_view_get_view(MediaControllerView* media_controller_view);

void media_controller_view_set_connected_status(
    MediaControllerView* media_controller_view,
    bool connected);
