#ifndef SCENE_HEADERS
#define SCENE_HEADERS

#include "flipper.h"
#include "protocols.h"

#include "scene_main_menu.h"
#include "scene_sub_menu.h"
#include "scene_about.h"
#include "scene_info.h"
#include "scene_dcf77.h"
#include "scene_msf.h"

/** The current scene */
typedef enum {
    LWCMainMenuScene,
    LWCSubMenuScene,
    LWCDCF77Scene,
    LWCMSFScene,
    LWCInfoScene,
    LWCAboutScene,
    __lwc_number_of_scenes
} LWCScene;

/** The current view */
typedef enum {
    LWCMainMenuView,
    LWCSubMenuView,
    LWCDCF77View,
    LWCMSFView,
    LWCInfoView,
    LWCAboutView
} LWCView;

extern void (*const lwc_scene_on_enter_handlers[])(void*);
extern bool (*const lwc_scene_on_event_handlers[])(void*, SceneManagerEvent);
extern void (*const lwc_scene_on_exit_handlers[])(void*);
extern const SceneManagerHandlers lwc_scene_manager_handlers;

bool lwc_custom_callback(void* context, uint32_t custom_event);
bool lwc_back_event_callback(void* context);
void lwc_tick_event_callback(void* context);

#endif
