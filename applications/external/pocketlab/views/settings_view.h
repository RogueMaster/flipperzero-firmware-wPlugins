#pragma once

#include <gui/view.h>
#include <notification/notification.h>

#include "../helpers/pocketlab_storage.h"

typedef struct SettingsView SettingsView;

// Invoked when the user picks "Reset progress"; the scene shows the confirm page.
typedef void (*SettingsViewResetCallback)(void* context);

SettingsView* settings_view_alloc(void);
void settings_view_free(SettingsView* instance);
View* settings_view_get_view(SettingsView* instance);

void settings_view_set_reset_callback(
    SettingsView* instance,
    SettingsViewResetCallback callback,
    void* context);

// Point the view at the live app state (mutated in place) and the notifier.
void settings_view_configure(
    SettingsView* instance,
    PocketLabState* state,
    NotificationApp* notifications);
