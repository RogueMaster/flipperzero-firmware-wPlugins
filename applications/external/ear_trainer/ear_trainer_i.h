#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>

#include "ear_events.h"
#include "helpers/curriculum.h"
#include "helpers/intervals.h"
#include "helpers/progress.h"
#include "helpers/tone_player.h"
#include "views/quiz_view.h"

#define ET_TAG "EarTrainer"

/* Quiz length scales with how much the level introduces: each new interval
 * gets guaranteed repetitions, plus review questions drawn from everything
 * learned so far. */
#define QUIZ_REPS_PER_NEW   3
#define QUIZ_REVIEW_EXTRA   5
#define CHALLENGE_QUESTIONS 15
#define CHALLENGE_MAX_MISS  3
#define HINTS_PER_QUIZ      2
#define FEEDBACK_RIGHT_MS   900
#define FEEDBACK_WRONG_MS   2200

typedef enum {
    EarSceneMenu,
    EarSceneLevelSelect,
    EarSceneTeach,
    EarSceneQuiz,
    EarSceneResults,
    EarSceneProgress,
    EarSceneReference,
    EarSceneSettings,
    EarSceneAbout,
    EarSceneCount,
} EarSceneId;

typedef enum {
    EarViewSubmenu,
    EarViewWidget,
    EarViewQuiz,
    EarViewVarItemList,
} EarViewId;

typedef struct {
    bool challenge;
    uint8_t q_index; /* 0-based question number */
    uint8_t total_questions;
    uint8_t pass_score; /* 80% of total, rounded up */
    uint8_t star2_score; /* 90% of total, rounded up */
    uint8_t hints_left;
    uint8_t correct;
    uint8_t mistakes; /* challenge only */
    uint8_t streak;

    uint8_t root_midi; /* first note of the current question */
    uint8_t answer; /* semitones being tested */
    bool descending; /* second note below the first */
    uint8_t last_answer; /* avoids asking the same interval twice running */

    uint8_t pool[IntervalCount]; /* intervals unlocked for this level */
    uint8_t pool_count;
    uint8_t queue[64]; /* pre-built question order */
    uint8_t queue_len;

    bool finished;
    bool passed;
    uint8_t stars;
    bool quit_armed;
} QuizState;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    Widget* widget;
    VariableItemList* var_item_list;
    QuizView* quiz_view;
    TonePlayer* player;
    FuriTimer* feedback_timer;

    EarProgress progress;
    EarSettings settings;
    uint8_t mode; /* TrainMode */
    uint8_t level; /* 0-based */
    uint8_t teach_index; /* which new interval the teach screen is showing */
    uint8_t reference_index; /* selection on the reference screen */
    QuizState quiz;
    QuizModel qm; /* scratch snapshot pushed into the quiz view */
} EarTrainerApp;
