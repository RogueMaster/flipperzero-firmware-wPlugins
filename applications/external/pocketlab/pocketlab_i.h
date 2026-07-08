#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>

#include "scenes/pocketlab_scene.h"
#include "views/home_view.h"
#include "views/lesson_view.h"
#include "views/labs_list_view.h"
#include "views/progress_view.h"
#include "views/about_view.h"
#include "helpers/pocketlab_content.h"
#include "helpers/pocketlab_i18n.h"
#include "helpers/pocketlab_sound.h"
#include "helpers/pocketlab_storage.h"

typedef enum {
    PocketLabViewHome,
    PocketLabViewWidget,
    PocketLabViewSettings,
    PocketLabViewLabs,
    PocketLabViewProgress,
    PocketLabViewLesson,
    PocketLabViewAbout,
} PocketLabViewId;

typedef enum {
    PocketLabCustomEventLessonDone = 0x1000,
    PocketLabCustomEventResetConfirm,
    PocketLabCustomEventResetCancel,
} PocketLabCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    HomeView* home_view;
    Widget* widget;
    VariableItemList* variable_item_list;
    LabsListView* labs_list_view;
    ProgressView* progress_view;
    LessonView* lesson_view;
    AboutView* about_view;

    PocketLabState state;
    const PocketLabLab* current_lab;
} PocketLab;

/** Award XP and unlock the badge for a lab the first time it is completed. */
void pocketlab_award_lab(PocketLab* app, const PocketLabLab* lab);

/** Clear all XP, level and completed labs, then persist. */
void pocketlab_reset_progress(PocketLab* app);

/** True when the lab has already been completed. */
bool pocketlab_is_lab_completed(const PocketLab* app, const PocketLabLab* lab);

/** Number of labs completed so far. */
uint8_t pocketlab_completed_count(const PocketLab* app);

/** Level reached for a given XP total. */
uint32_t pocketlab_level_for_xp(uint32_t xp);
