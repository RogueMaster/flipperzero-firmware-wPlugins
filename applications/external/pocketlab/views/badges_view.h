#pragma once

#include <gui/view.h>
#include <notification/notification.h>

typedef struct BadgesView BadgesView;

BadgesView* badges_view_alloc(void);

void badges_view_free(BadgesView* instance);

View* badges_view_get_view(BadgesView* instance);

/** Load which badges are earned (bitmask matching the lab order). */
void badges_view_configure(
    BadgesView* instance,
    uint64_t completed_mask,
    NotificationApp* notifications,
    bool sound_enabled);
