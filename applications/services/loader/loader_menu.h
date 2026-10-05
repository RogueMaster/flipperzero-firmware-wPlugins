#pragma once
#include <furi.h>
#include "loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAINMENU_APPS_PATH INT_PATH(".mainmenu_apps.txt")

enum {
    LoaderMenuIndexApplications = (uint32_t)-1,
    LoaderMenuIndexLast = (uint32_t)-2,
    LoaderMenuIndexSettings = (uint32_t)-3,
};

typedef struct LoaderMenu LoaderMenu;

LoaderMenu*
    loader_menu_alloc(void (*closed_cb)(void*), void* context, bool settings_only, bool games_only);

/** Firmware-internal entry point; opens Game Menu independently of desktop mode. */
void loader_show_games_menu(Loader* loader);

void loader_menu_free(LoaderMenu* loader_menu);

#ifdef __cplusplus
}
#endif
