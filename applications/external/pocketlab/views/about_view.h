#pragma once

#include <gui/view.h>
#include <notification/notification.h>

typedef struct AboutView AboutView;

AboutView* about_view_alloc(void);

void about_view_free(AboutView* instance);

View* about_view_get_view(AboutView* instance);

/** Wire up the printing sounds and (re)start the intro from the top. */
void about_view_configure(AboutView* instance, NotificationApp* notifications, bool sound_enabled);
