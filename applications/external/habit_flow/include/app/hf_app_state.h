#pragma once

#include "include/application/hf_session_service.h"
#include "include/domain/habit.h"
#include "include/persistence/habit_store.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Gui Gui;
typedef struct ViewDispatcher ViewDispatcher;
typedef struct View View;
typedef struct TextInput TextInput;
typedef struct Widget Widget;
typedef struct DialogEx DialogEx;
typedef struct Popup Popup;

typedef enum {
    HfViewMain = 0,
    HfViewCredits,
    HfViewDetail,
    HfViewManage,
    HfViewEdit,
    HfViewTextInput,
    HfViewDialog,
    HfViewPopupMastered,
} HfViewId;

typedef enum {
    HfDlgNone = 0,
    HfDlgYesterday,
    HfDlgResetStreak,
    HfDlgDeleteHabit,
} HfDlgMode;

typedef struct HabitFlowApp HabitFlowApp;

typedef struct {
    HabitFlowApp *app;
} HfCtx;

struct HabitFlowApp {
    HabitStore store;
    ViewDispatcher *vd;
    Gui *gui;
    View *main;
    View *detail;
    View *manage;
    View *edit;
    TextInput *text_input;
    Widget *credits;
    DialogEx *dialog;
    Popup *popup_mastered;
    char credits_buf[320];

    uint32_t list_sel;
    uint32_t list_scroll;
    uint32_t detail_index;
    uint32_t manage_sel;
    uint32_t manage_scroll;
    uint8_t edit_row;
    Habit edit_buf;
    bool edit_is_new;
    size_t edit_index;
    char text_buf[HF_HABIT_NAME_MAX];

    HfYesterdayQueue yq;

    HfDlgMode dlg_mode;
    HfViewId current;
};
