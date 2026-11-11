#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include "core/pool.h"
#include "core/progress.h"
#include "core/session.h"
#include "scenes/hamcram_scene.h"
#include "views/quiz_view.h"

#define HAMCRAM_VERSION    FAP_VERSION
#define HAMCRAM_MAX_POOLS  8
#define HAMCRAM_POOL_DIR   APP_ASSETS_PATH("pools")
#define HAMCRAM_POOL_EXT   ".hcp"
#define HAMCRAM_PATH_MAX   80
/* Characters per line of body text that reliably fit the screen width */
#define HAMCRAM_WRAP_CHARS 23

typedef enum {
    HamCramViewSubmenu,
    HamCramViewWidget,
    HamCramViewDialog,
    HamCramViewVariableItemList,
    HamCramViewQuiz,
} HamCramView;

/* Custom events. Submenu selections use their item index, which is always
 * below HamCramEventBase. */
typedef enum {
    HamCramEventBase = 1000,
    HamCramEventQuizAnswer, /* + displayed slot 0-3 */
    HamCramEventQuizNext = HamCramEventQuizAnswer + 4,
    HamCramEventQuizPrev,
    HamCramEventLeft,
    HamCramEventCenter,
    HamCramEventRight,
} HamCramEvent;

typedef enum {
    HamCramConfirmExam,
    HamCramConfirmReset,
} HamCramConfirm;

typedef struct {
    char file[32]; /* File name inside HAMCRAM_POOL_DIR */
    char name[16];
    char years[12];
    uint8_t element;
    uint32_t valid_from;
    uint32_t valid_to;
} HamCramPoolEntry;

typedef struct {
    bool skip_figures;
    bool shuffle;
    bool feedback; /* LED and vibration when an answer is revealed */
} HamCramSettings;

typedef struct {
    SessionMode mode;
    uint16_t correct;
    uint16_t total;
    uint16_t need;
    bool passed;
} HamCramResult;

typedef struct {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    Widget* widget;
    DialogEx* dialog;
    VariableItemList* variable_item_list;
    QuizView* quiz_view;
    FuriString* text;

    HamCramSettings settings;
    HamCramPoolEntry pools[HAMCRAM_MAX_POOLS];
    uint8_t pool_count;
    uint32_t today; /* YYYYMMDD from the clock */

    /* The open pool */
    Pool* pool;
    Progress* progress;
    Session* session;
    const HamCramPoolEntry* pool_entry;
    char save_path[HAMCRAM_PATH_MAX];

    /* State shared between scenes */
    uint8_t topic; /* Subelement chosen in the topic list */
    uint8_t quiz_correct; /* Original index of the right answer on screen */
    bool quiz_loaded;
    bool warned_no_pools;
    HamCramResult result;
    const char* message_header;
    const char* message_text;
    char line[2][64];
} HamCram;

/* Pool lifecycle (hamcram.c) */
bool hamcram_pool_expired(const HamCram* app, const HamCramPoolEntry* entry);
bool hamcram_open_pool(HamCram* app, uint8_t index);
void hamcram_close_pool(HamCram* app);
void hamcram_save_progress(HamCram* app);
void hamcram_save_settings(HamCram* app);

/** Blink green for a right answer; blink red and buzz for a wrong one */
void hamcram_feedback(HamCram* app, bool correct);

/** The same for a graded exam: two green blinks for a pass */
void hamcram_feedback_exam(HamCram* app, bool passed);

/** Shared submenu callback: sends the item index as a custom event */
void hamcram_submenu_callback(void* context, uint32_t index);

/** Start studying questions [first, first + count) and open the quiz */
void hamcram_start_study(HamCram* app, uint16_t first, uint16_t count);

/** Append `text` to `out`, wrapped at word boundaries. The stock scrolling
 * text widget would otherwise break lines in the middle of words. */
void hamcram_cat_wrapped(FuriString* out, const char* text);

/** Show a one-button notice; OK or Back returns to the current scene */
void hamcram_show_message(HamCram* app, const char* header, const char* text);
