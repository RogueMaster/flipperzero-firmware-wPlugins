#ifndef SCENE_MAIN_MENU_HEADERS
#define SCENE_MAIN_MENU_HEADERS

#include "flipper.h"

typedef enum {
    LWCMainMenuDCF77,
    LWCMainMenuMSF,
    LWCMainMenuInfo,
    LWCMainMenuAbout,
} LWCMainMenuSceneIndex;

typedef enum {
    LWCMainMenuSelectDCF77,
    LWCMainMenuSelectMSF,
    LWCMainMenuSelectInfo,
    LWCMainMenuSelectAbout,
} LWCMainMenuEvent;

void lwc_main_menu_scene_on_enter(void* context);
bool lwc_main_menu_scene_on_event(void* context, SceneManagerEvent event);
void lwc_main_menu_scene_on_exit(void* context);

#endif
