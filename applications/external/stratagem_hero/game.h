#pragma once

#include <gui/view.h>
#include "settings.h"

typedef enum {
    StrataHeroGameWidgetNavigationEvent_Back
} StrataHeroGameWidgetNavigationEvent;

typedef struct StrataHeroGameWidget StrataHeroGameWidget;
typedef void (*StrataHeroGameWidgetNavigationCallback)(StrataHeroGameWidgetNavigationEvent, void*);

StrataHeroGameWidget* stratahero_game_widget_alloc();
void stratahero_game_widget_free(StrataHeroGameWidget* widget);
View* stratahero_game_widget_get_view(StrataHeroGameWidget* widget);

void stratahero_game_widget_set_settings(StrataHeroGameWidget* widget, StrataHeroSettings* settings);

void stratahero_game_widget_set_navigation_callback(
    StrataHeroGameWidget* widget,
    StrataHeroGameWidgetNavigationCallback callback,
    void* context
);

int stratahero_game_widget_get_score(StrataHeroGameWidget* widget);

