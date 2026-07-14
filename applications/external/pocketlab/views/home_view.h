#pragma once

#include <gui/view.h>
#include <notification/notification.h>

#define HOME_VIEW_MAX_ITEMS 6

typedef struct HomeView HomeView;

typedef void (*HomeViewCallback)(void* context, uint32_t index);

HomeView* home_view_alloc(void);

void home_view_free(HomeView* instance);

View* home_view_get_view(HomeView* instance);

void home_view_set_callback(HomeView* instance, HomeViewCallback callback, void* context);

/** Set the header title, the menu items, the starting cursor and the XP total. */
void home_view_configure(
    HomeView* instance,
    const char* title,
    const char* const* items,
    size_t count,
    uint32_t selected,
    uint32_t xp,
    NotificationApp* notifications,
    bool sound_enabled);

/** Current cursor position, so the scene can remember it. */
uint32_t home_view_get_selected(HomeView* instance);
