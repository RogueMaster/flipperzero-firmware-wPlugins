/* Home screen: transport status header + a Submenu-style list. SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <gui/view.h>

typedef struct HomeView HomeView;
typedef void (*HomeViewCallback)(void* context, uint32_t index);

typedef struct {
    bool board_ok; /* ESP32 answered (or mock) */
    bool demo; /* mock backend active */
    bool lan_up;
    bool wifi_up;
    char text[32]; /* right side: IP, "No board", "No network", ... */
} HomeStatus;

HomeView* home_view_alloc(void);
void home_view_free(HomeView* home);
View* home_view_get_view(HomeView* home);
void home_view_set_items(HomeView* home, const char* const* labels, uint8_t count);
void home_view_set_callback(HomeView* home, HomeViewCallback cb, void* context);
void home_view_set_status(HomeView* home, const HomeStatus* status);
void home_view_set_selected(HomeView* home, uint8_t index);
uint8_t home_view_get_selected(HomeView* home);
