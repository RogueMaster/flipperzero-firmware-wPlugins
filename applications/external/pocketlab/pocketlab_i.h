#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>

#include "scenes/pocketlab_scene.h"
#include "views/home_view.h"
#include "views/lesson_view.h"
#include "views/labs_list_view.h"
#include "views/progress_view.h"
#include "views/badges_view.h"
#include "views/levelup_view.h"
#include "views/exam_view.h"
#include "views/about_view.h"
#include "views/settings_view.h"
#include "views/reset_view.h"
#include "helpers/pocketlab_content.h"
#include "helpers/pocketlab_fonts.h"
#include "helpers/pocketlab_i18n.h"
#include "helpers/pocketlab_sound.h"
#include "helpers/pocketlab_storage.h"

typedef enum {
    PocketLabViewHome,
    PocketLabViewWidget,
    PocketLabViewSettings,
    PocketLabViewLabs,
    PocketLabViewProgress,
    PocketLabViewBadges,
    PocketLabViewLesson,
    PocketLabViewAbout,
    PocketLabViewLevelUp,
    PocketLabViewExam,
    PocketLabViewReset,
} PocketLabViewId;

typedef enum {
    PocketLabCustomEventLessonDone = 0x1000,
    PocketLabCustomEventResetConfirm,
    PocketLabCustomEventResetCancel,
    PocketLabCustomEventOpenBadges,
    PocketLabCustomEventLevelUpDone,
    PocketLabCustomEventExamDone,
    PocketLabCustomEventSettingsReset,
} PocketLabCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    HomeView* home_view;
    Widget* widget;
    SettingsView* settings_view;
    LabsListView* labs_list_view;
    ProgressView* progress_view;
    BadgesView* badges_view;
    LessonView* lesson_view;
    AboutView* about_view;
    LevelUpView* levelup_view;
    ExamView* exam_view;
    ResetView* reset_view;

    PocketLabState state;
    const PocketLabLab* current_lab;
} PocketLab;

#define POCKETLAB_XP_MAX 9999 // hard cap; the XP display glitches at this value

/** Add XP (clamped to POCKETLAB_XP_MAX) and update the level. Returns true if
 * the level went up. Does not persist. */
bool pocketlab_add_xp(PocketLab* app, uint32_t amount);

/** Award XP/badge (first completion) and bump the daily streak. Returns true if
 * this completion raised the level. */
bool pocketlab_award_lab(PocketLab* app, const PocketLabLab* lab);

/** Clear all XP, level and completed labs, then persist. */
void pocketlab_reset_progress(PocketLab* app);

/** True when the lab has already been completed. */
bool pocketlab_is_lab_completed(const PocketLab* app, const PocketLabLab* lab);

/** Number of labs completed so far. */
uint8_t pocketlab_completed_count(const PocketLab* app);

/** Level reached for a given XP total. */
uint32_t pocketlab_level_for_xp(uint32_t xp);

/** Human-readable title for a level (Novice, Apprentice, ...). */
const char* pocketlab_level_title(uint32_t level);
